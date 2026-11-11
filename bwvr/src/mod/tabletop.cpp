// Tabletop mode: the island as a model on a table in the room.
//
// The player walks around the table and leans over it. The table is about
// 1.5 m across, so a land of 1400 to 3300 units gets about 2000 units per
// meter, and a head step of 1 cm moves the eye 20 units. The design is
// docs/tabletop.md.
//
// THE CAMERA. The table maps the room to the world:
//
//     world = C + s * Lt * (room - Tc)
//
// head_camera.cpp already turns the room into the world for the life size
// view. On the main call of a frame, this file gives it a camera at P0, the
// place of the recenter pose in the world, that looks along the table's
// heading. It also gives it the gain that makes the lean s units per meter.
// The head then goes on that camera as it does for the life size view, so the
// render head is
// E = P0 + Lt * t. See TabletopCamera.
//
// THE GAME KEEPS ITS OWN CAMERA. The game logic reads the stored camera at
// 0x00EA1DB8: the sound, the scripts and the help. At the table scale the eye
// is thousands of units from the game camera. So the stored camera holds the
// render head only inside GGame::ProcessGraphicsEngine, where the Hand, the
// pick and the interface need it. After that function the game gets its own
// camera again. See TabletopGraphicsDone.
//
// THE ENGINE IS TUNED FOR A NEAR CAMERA. The fog ends at 900 units, the map
// objects stop at 700, the land detail drops at 1600, and the Hand stops at
// 1800. TabletopApplyFrame changes each value while the table is on, and
// puts back the game's value when it goes off.
//
// THE EDGE. A hook on the per block cull drops land blocks past the radius
// of the table. The plain sea is drawn as a disc of that radius, not as a
// quad of 140000 units. Clouds past the edge are not drawn.
//
// THE KEY COLOR. With table_key_color set, a clear in that color covers the
// screen past the edge of the table, for the passthrough mask of Virtual
// Desktop. See h_SkyDraw.
//
// THE HAND, THE EARS AND THE GRAB. controllers.cpp puts the direct hand on
// the table through TabletopRoom, and hand_direct.cpp gives the Hand the
// size of TabletopHandSize. audio_listener.cpp puts the ears at
// TabletopEars. The grab of world_grab.cpp moves the land on the table
// through TabletopMove: C, psi and s change, and the table top stays where
// it is in the room. See docs/tabletop.md, "The hand on the table", "Moving
// the table by the grips" and "The ears on the table".
//
// Nothing else moves the table. Only the player's own head and hands move
// the view.
//
// THE GOD VIEW (play_mode=4, docs/god-view.md) is the same camera with a
// different room. The table center in the room is right under the recenter
// pose, god_ground_down_cm below it, so the land under the player is at the
// waist, and there is no edge: no cut, no disc, no key color. The scale goes
// from the life size scale to the table scales. The right stick zooms about
// the land that the player looks at (god_zoom_about, GodZoomPoint), in steps
// or while it is held (god_zoom_smooth). The other hand's grip raises and
// lowers the ground, and a snap turn turns
// the view about the head. Each step is used once, at the next main camera
// call (ApplyGodSteps), and the smooth zoom changes the scale once a frame
// there (ApplyGodZoomHold). The Hand is the real hand at the scale. The fog,
// the detail distances and the far limits follow the scale, and a hook on
// the near plane keeps the real hand in view at the life size scale. The
// game's own camera stays over the player's place at a normal play distance,
// and comfort_camera.cpp writes it (TabletopGodGameCamera). When the game
// puts its own camera at a new place, the view goes there as one cut: the
// land point that the camera looks at is in front of the player's head
// (PlaceGod, docs/god-view.md, "The place at a camera of the game").

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "MinHook.h"
#include "../common/bw_addresses.h"
#include "blinder_math.h"
#include "camera_zone.h"
#include "comfort_math.h"
#include "cull_math.h"
#include "debug_menu.h"
#include "hint_card.h"
#include "hint_learn_math.h"
#include "table_math.h"
#include "tabletop.h"
#include "solid_math.h"
#include "walk_math.h"
#include "zoom_math.h"

extern void ModLog(const char* format, ...);
// mod_main.cpp: the predicted display time of the open frame of the headset,
// in seconds, or 0 with no session.
double ModDisplaySeconds();
// head_camera.cpp: build the matrices and the stored camera from the game
// camera of the last main call.
bool HeadCameraUseGameCamera();
bool HeadCameraEnabled();
// head_pose.cpp: the life size scale, in units per meter, and the last head
// sample: the head matrix and its lean from the recenter pose, in units at
// the life size scale.
float HeadPoseScale();
// The count of the recenters of the room.
uint64_t HeadPoseOriginSerial();
bool HeadPoseCurrent(float matrix_out[9], float translation_out[3]);
// stereo.cpp: the display time of the last frame of the headset and the
// display period, in nanoseconds. False with no headset.
bool StereoDisplayTime(int64_t* time, int64_t* period);
// frame_timers.cpp hooks GGame::ProcessGraphicsEngine, and calls
// TabletopGraphicsDone after it.
bool FrameTimersGraphicsHooked();
// head_camera.cpp: the render head of the last table frame, in world units.
bool HeadCameraTableEye(float out[3]);
// controllers.cpp: the real hand's length in cm (hand_real_cm) and the
// percent of it that the Hand gets (hand_scale_pct).
int ControllersHandCm();
int ControllersHandScalePct();
// comfort_camera.cpp: a short black blink after a step, when the player
// asked for one (blink). It does nothing with the blink off.
void ComfortCameraBlink();
// script_camera.cpp: the last main camera call belonged to a story or a
// follow mode.
bool ScriptCameraStoryOwns();
// cutscene_window.cpp: this frame draws on the screen of the cut scene window.
bool CutsceneWindowFlat();
// hand_direct.cpp: the answers that the scripts of the game got from the
// player since the last call (docs/god-view.md, "The player that a script
// sees").
uint64_t HandDirectTakeScriptAnswers();
// pointer_map.cpp: the ray of the mouse in the room, while the mouse owns
// the pointer.
bool PointerMapMouseRay(float dir_room[3]);
// comfort_camera.cpp: the update of the camera mode in this frame had a land
// drag in the God view. It is read once.
bool ComfortCameraTakeGodDrag();
// comfort_camera.cpp: where the game camera goes, from the last update of the
// player's camera mode. False when that mode did not run just now.
bool ComfortCameraDestinations(float position[3], float focus[3]);
// controllers.cpp: a grip holds the world.
bool ControllersGrabHeld();
// head_blinder.cpp: head_blinder is on, the level of its overlay (0 to 1),
// and the clearance of the head with what it is near.
bool HeadBlinderOn();
float HeadBlinderLevel();
float HeadBlinderClearance(const char** what);
// hand_direct.cpp: the solids near the hands and the head that the first eye
// pass of the frame before kept, and the class of one for the log.
int HandDirectSolids(void* objects[], solid_math::Solid out[], int max);
const char* HandDirectSolidClass(void* object);
// vignette.cpp: a move of the view that the player makes with a control, with
// the share of its full speed, and a step of the view in one frame.
void VignetteMove(float share, const char* by);
void VignetteStep(const char* by);
// teleport.cpp: the camera help event of a jump (0x305 to the land, 0x306 to
// an object under the Hand).
int TeleportJumpEvent();
// controllers.cpp: a pulse of the pointing hand for a test key, and the frame
// number that the scripted controller counts.
void ControllersGesturePulse(float amplitude, float seconds);
uint64_t ControllersFrameCount();
// hand_direct.cpp: the point of the real hand in the world, from the frame
// before. False while no controller places the Hand.
bool HandDirectRealPoint(float out[3]);
// cull_distance.cpp: the latch of the cull distance for this frame, its
// factor of the map object distance, and its report.
void CullDistanceFrame();
float CullDistanceFactor();
void CullDistanceSetFog(float ratio);
void CullDistanceReport();

namespace {

using namespace table_math;

// The per block cull, fastcall with the block in ecx and a plain ret. Its
// only caller is LH3DIsland::PreDraw, once per block in each eye pass.
const uintptr_t kBlockCull = 0x00877210;
// LH3DSky::Draw, cdecl with no arguments, plain ret at 0x0086B6AA.
const uintptr_t kSkyDraw = 0x0086B010;
// The call of the plain sea (fn_0087A090) to the triangle submit
// fn_0081C090. The submit takes the vertex count in ecx and the positions in
// edx, then six stack arguments: colors, texture points, the triangle count,
// the indices, the material and the flags. It ends in ret 0x18.
const uintptr_t kSeaCall = 0x0087A2CD;
const uintptr_t kSubmit = 0x0081C090;
const uint8_t kSeaCallBytes[5] = {0xE8, 0xBE, 0x1D, 0xFA, 0xFF};

// The land blocks. The pointer table starts at 0x00E9C564 and has 256
// entries. Entry 0 is not a block, and LH3DIsland::PreDraw walks entries 1
// to count - 1.
const uintptr_t kBlockTable = 0x00E9C564;
const uintptr_t kBlockCount = 0x00E9CD70;
const int kMaxBlocks = 256;
const int kBlockX = 0x90C;       // float, the corner of the block
const int kBlockZ = 0x910;
const int kBlockDetail = 0x930;  // the reject path of the cull writes these
const int kBlockFlags = 0x920;
const int kBlockState = 0x934;
const float kBlockSize = 160.0f;
const int kBlockCells = 16;
const float kCellSize = 10.0f;
const int kMapBlocks = 32;
const float kMapSize = 5120.0f;

// LH3DIsland::GetAltitude, fastcall with the map coordinates, the height in
// st0. The coordinates are ints of x * 6553.6 and z * 6553.6. A cell with an
// altitude byte of 3 or less is sea and reads as 0.
const uintptr_t kGetAltitude = 0x00803090;
const float kMapScale = 6553.6f;
typedef float(__fastcall* GetAltitudeFn)(const int32_t* coords);

// The game object, and the camera it draws from. Not 0 in the Temple and in
// other rooms, which draw from a camera of their own.
const uintptr_t kGame = 0x00D0195C;
const uintptr_t kGameCameraSource = 0x00205A28;
// GGame::MyInterface, thiscall. The CHand is at interface + 0x3A0.
const uintptr_t kMyInterface = 0x00555850;
const int kFaceHand = 0x3A0;
typedef void*(__fastcall* MyInterfaceFn)(void* game, void* unused);

// The values of docs/tabletop.md, "What the engine does at the table scale".
const uintptr_t kFog = 0x00C37204;          // int, the Fog setting of BWSetup
// The fog start and end of the land and the objects, in units of depth.
// The sky writes them each frame (docs/tabletop.md, "The mechanisms").
const uintptr_t kFogStart = 0x00C37220;     // float
const uintptr_t kFogEnd = 0x00C37224;       // float
const uintptr_t kVanish = 0x00C37200;       // float, 700
const uintptr_t kSizeCap = 0x00C37EC8;      // float, 5
const uintptr_t kDetailNear = 0x00E9C4FC;   // float, 1600
const uintptr_t kDetailFar = 0x00E9C4D4;    // float, 2500
const uintptr_t kTextureLine = 0x00E9C524;  // float, 400 to 800
const uintptr_t kReflection = 0x00E9CD8C;   // int
const int kHandReach = 0x4838;              // float in the CHand, 1800
// CHand::SetDistanceFromView makes the Hand's size 0 at 5650 units and turns
// it inside out past that. So the reach stops a little short.
const float kReach = 5600.0f;
const float kFar = 1.0e6f;
const float kObjectsFar = 8000.0f;
const float kSizeCapFar = 30.0f;
// The zoom of the grab stops here: about 40 units per meter, where a village
// fills the table (docs/tabletop.md, "Moving the table by the grips"). The other end is the scale at the
// start, which fits the whole land.
const float kLowestScale = 40.0f;
// The ears stay at most this far over the land. The game's own camera is
// about this far from what it looks at.
const float kEarsAbove = 400.0f;

// What the design names as risks. 1 when the Z buffer is 16 bit. The object
// list of GLandscape stops at 2999, and the sorted list of transparent
// objects at 2048, both with no message.
const uintptr_t kZBuffer16 = 0x00C386D8;
const uintptr_t kObjectCount = 0x00D20198;  // GLandscape::DrawObjectCount
const int32_t kObjectLimit = 2999;
const uintptr_t kSortedCount = 0x00EE9D34;  // LH3DZSorter, reset at StartFrame
const int32_t kSortedLimit = 2048;

// The mist dome's position.
const int kMistPosition = 0x38;

// THE GOD VIEW.
//
// The near plane of the God view is at most 5 cm of the room, at the scale
// of TabletopViewScale. near_plane.cpp hooks LandFeature::GetNearClipping
// for that, in play modes 0, 3 and 4.
// At this gain (the scale over the life size scale) and above, the fog and
// the reflection go off, as on the table. Below it the game keeps its own.
// The fog ends at 900 units, which at the gain of 3 is 40 m of the room.
const float kGodFarGain = 3.0f;
// The game camera over the player's place: 400 units from the ground under
// the head, pitched down 50 degrees (docs/tabletop.md, "The game camera on the
// table"). The ground
// point stays within 3000 units of the map center, well inside the pull
// back of GCamera::Update at 3500.
const float kGodGameDistance = 400.0f;
const float kGodGamePitch = 50.0f / 57.29578f;
const float kGodGameRadius = 3000.0f;
const float kMapCenter = 2560.0f;
// A step of the ground keeps the land under the head between 10 cm and 2.5 m
// below the head. A step that would go past a limit is refused. The level of
// a zoom is the height of the land under the head, kept in the same range
// (GodZoomPoint), so a hill over the head goes down as the view zooms out.
const float kGroundNearest = 0.10f;
const float kGroundFarthest = 2.50f;
// No edge in the God view.
const float kNoEdge = 1.0e9f;

// THE GAME'S CAMERA SWITCHES, CameraHelp::EnabledFeatures. Scripts and the
// tutorial turn the camera controls on and off with them. Bit 2 lets the
// camera turn, bit 4 lets it zoom, and bit 8 lets the player drag the land
// (docs/god-view.md, "The game camera"). The table and the God view keep to
// them.
const uintptr_t kEnabledFeatures = 0x009CDD6C;
const uint32_t kFeatureTurn = 0x2;
const uint32_t kFeatureZoom = 0x4;
const uint32_t kFeatureDrag = 0x8;
// CameraHelp::CameraHelpCallback, cdecl (event, point, inputs). The view
// sends the events that the game sends for its own camera, so a tutorial
// step that waits for one can end: a turn (0x300, then 0x301 to the left or
// 0x302 to the right), a zoom (0x304) and a drag of the land (0x308). The
// inputs: 2 is the left button and 8 is the wheel. See comfort_camera.cpp.
const uintptr_t kCameraHelpCallback = 0x00449140;
typedef void(__cdecl* HelpFn)(int event, const void* point, unsigned inputs);
const int kHelpTurn = 0x300;
const int kHelpTurnLeft = 0x301;
const int kHelpTurnRight = 0x302;
const int kHelpZoom = 0x304;
const int kHelpDrag = 0x308;
const unsigned kInputButton = 0x02;
const unsigned kInputWheel = 0x08;
// THE MOUSE PAN. The ray of the press looks for the land in steps of 2 units
// of the world, up to the reach of the Hand (kReach).
const float kPanStepUnits = 2.0f;
// GGame::ClearMap, thiscall with no arguments and a plain ret. LoadAllGame,
// StartPlaygroundGame and GGame::Close call it.
const uintptr_t kClearMap = 0x00552BB0;
typedef void(__fastcall* ClearMapFn)(void* game, void* unused);
ClearMapFn o_ClearMap = nullptr;

typedef void(__fastcall* BlockCullFn)(uint8_t* block);
typedef void(__cdecl* SkyDrawFn)();
typedef void(__fastcall* SubmitFn)(int count, const float* positions,
                                   const uint32_t* colors, const float* uvs,
                                   int triangles, const void* indices,
                                   void* material, int flags);
// IDirect3DDevice7::Clear, slot 10.
typedef HRESULT(__stdcall* ClearFn)(void* device, DWORD count, void* rects,
                                    DWORD flags, DWORD color, float z,
                                    DWORD stencil);
const int kClearSlot = 10;
const DWORD kClearTarget = 1;

BlockCullFn o_BlockCull = nullptr;
SkyDrawFn o_SkyDraw = nullptr;

TabletopSettings g_settings;
bool g_installed = false;
// The player's switch. Ctrl F11 turns it off and on.
bool g_on = true;
// The table owns this frame. Set at the main camera call, and cleared at the
// end of GGame::ProcessGraphicsEngine.
bool g_live = false;
// The last main camera call was the table's. Unlike g_live it holds between
// two frames, for the grab and the ears, which run outside the bracket.
bool g_owned = false;

// THE LAND, measured once for each land: its middle and the scale that fits
// it on the table.
bool g_have_fit = false;
uint32_t g_signature[4] = {0, 0, 0, 0};
float g_fit_center[3] = {0.0f, 0.0f, 0.0f};
float g_far_radius = 0.0f;
float g_fit_scale = 0.0f;
bool g_block_land[kMapBlocks][kMapBlocks];

// THE TABLE in the world, built from the fit when the mode starts. The grab
// moves C, psi and s. The rest follows from them (Derive).
bool g_have_table = false;
float g_center[3] = {0.0f, 0.0f, 0.0f};
float g_heading = 0.0f;
float g_scale = 0.0f;
float g_radius = 0.0f;
float g_basis[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
float g_origin[3] = {0.0f, 0.0f, 0.0f};
float g_focus_distance = 1.0f;
// Goes up each time the table is built again, or a step of the God view
// moves it, so a grab anchors again.
uint64_t g_serial = 0;

// THE GOD VIEW. The steps the player asked for and that the next main camera
// call uses: 1, -1 or 0 each. The scale of the last God view, which it keeps
// when it is built again.
int g_god_zoom = 0;
// THE POINT OF A ZOOM (GodZoomPoint): the point in the world, how far its
// level is below the head, which point it is (zoom_math::PointKind), and how
// far it is from the head across the room, in meters.
struct ZoomPoint {
  float pivot[3] = {0.0f, 0.0f, 0.0f};
  float below = 0.0f;
  int kind = 0;
  float across = 0.0f;
};
// The check of a zoom step: the render head of the frame before the step, the
// scale before and after it, and the point of the zoom. The frame after the
// step compares them with the render head of the step frame.
struct ZoomProbe {
  bool armed = false;
  float eye[3] = {0.0f, 0.0f, 0.0f};
  float before = 1.0f;
  float scale = 1.0f;
  float pivot[3] = {0.0f, 0.0f, 0.0f};
} g_zoom_probe;
int g_god_ground = 0;
int g_god_turn = 0;
float g_god_turn_degrees = 45.0f;
// THE SMOOTH TURN (ApplyGodTurnHold, snap_turn=4). share is the push that
// the stick or a turn key asked for since the last main camera call (-1
// full left to 1 full right), rate the degrees a second of a full push, and
// fresh says that a request came. The rest is the hold that runs: its
// direction, the heading at its start, the degrees and the time of its
// frames. help is the degrees that comfort_camera.cpp did not take yet for
// the turn events of the tutorial. The check after a hold compares the
// heading kHoldCheckFrames frames after the stop with the heading at the
// stop. cpu is the time of the same frames from the clock of the CPU, for
// the log only.
struct TurnHold {
  float share = 0.0f;
  float rate = 0.0f;
  bool fresh = false;
  bool active = false;
  int direction = 0;
  float start_heading = 0.0f;
  float degrees = 0.0f;
  double seconds = 0.0;
  double cpu = 0.0;
  uint64_t frames = 0;
  float help = 0.0f;
  int help_direction = 0;
  int check_frames = 0;
  float check_heading = 0.0f;
  uint64_t check_version = 0;
} g_turn_hold;
uint64_t g_turn_holds = 0;
// The log has one line for each frame of the first kTurnFrameLogHolds holds,
// at most kTurnFrameLogFrames frames each: the heading of the view in that
// frame.
const uint64_t kTurnFrameLogHolds = 4;
const uint64_t kTurnFrameLogFrames = 240;
void StopTurnHold(const char* cause, bool view_stays);
// THE COMFORT VIGNETTE (vignette.cpp): the view at the end of the last main
// camera call, so the next call can measure what a grab of the world moved
// in between. have is false after the view was built again.
struct MotionMark {
  bool have = false;
  TablePose pose = {};
} g_motion;
// THE SMOOTH ZOOM (ApplyGodZoomHold). push is what the stick asked for since
// the last main camera call, and fresh says that it asked. The rest is the
// hold that runs: its direction, the scale and the land point under the head
// at its start and after its last frame, the render head of the frame before
// it, the time of its frames, and how many of them changed the scale. point
// is the point of the zoom at the start, with its place in the room then and
// after the last frame. kept says that each frame of the hold uses that
// point (god_zoom_about 1 and 2), so the point does not move while the head
// turns. With god_zoom_about=0 each frame takes the land under the head
// again.
struct ZoomHold {
  float push = 0.0f;
  bool fresh = false;
  bool active = false;
  int direction = 0;
  bool kept = false;
  ZoomPoint point;
  float point_start[3] = {0.0f, 0.0f, 0.0f};
  float point_now[3] = {0.0f, 0.0f, 0.0f};
  float sunk = 0.0f;  // how far ZoomClearHead put the land down, in meters
  float start_scale = 0.0f;
  float scale = 0.0f;
  float start_under[2] = {0.0f, 0.0f};
  float under[2] = {0.0f, 0.0f};
  // The land under the head, in meters below the head, at the start of the
  // hold and after its last frame that changed the scale.
  float start_below = 0.0f;
  float below = 0.0f;
  bool have_eye = false;
  float start_eye[3] = {0.0f, 0.0f, 0.0f};
  double seconds = 0.0;
  uint64_t frames = 0;
  uint64_t moved = 0;
} g_hold;
// A frame of the smooth zoom counts at most this long, so a frame that took
// long does not make a jump of the scale.
const float kHoldFrameCap = 0.05f;
// The check after a hold: this many frames after the stop, the log compares
// the scale and the render head with the ones at the stop.
const int kHoldCheckFrames = 30;
struct HoldCheck {
  int frames_left = 0;
  float scale = 0.0f;
  uint64_t version = 0;
  bool have_eye = false;
  float eye[3] = {0.0f, 0.0f, 0.0f};
} g_hold_check;
// The time of the last main camera call of the God view, in seconds, and 0
// before the first one.
double g_god_clock = 0.0;
// god_display_time: the display time of the headset at the last main camera
// call, in seconds, and the frames that took their time from it (for the
// report).
double g_god_display = 0.0;
uint64_t g_display_frames = 0;
uint64_t g_holds = 0;
double g_hold_seconds = 0.0;
void StopZoomHold(const char* cause, bool view_stays);
// A solid stopped the smooth zoom in this hold, and the log said so.
bool g_hold_solid_said = false;
float g_god_last_scale = 0.0f;
// A story holds the camera, and the God view stands aside.
bool g_god_story = false;
// Goes up each time the player moves the God view: a step or a grab. The game
// camera is written once for each value (TabletopGodGameCamera).
uint64_t g_god_version = 0;
// The last value of g_god_version that was not a snap turn alone. A turn
// alone keeps the focus of the game camera (TabletopGodTurnedOnly).
uint64_t g_god_nonturn_version = 0;
uint64_t g_god_steps = 0;
uint64_t g_god_refused = 0;
// THE TELEPORT request (TabletopGodTeleport, TabletopTableTeleport): the
// land point to go to, kept until the next main camera call, which uses it
// once.
struct TeleportRequest {
  bool pending;
  float target[3];
};
TeleportRequest g_tele = {};
uint64_t g_tele_moves = 0;
int g_god_logs = 0;
const int kMaxGodLogs = 60;
// The lines of a zoom step and of a smooth zoom hold have their own count. A
// zoom writes three lines, so they do not use the lines of the God view.
int g_zoom_logs = 0;
const int kMaxZoomLogs = 150;
// THE PLACE AT A CAMERA OF THE GAME (PlaceGod, docs/god-view.md, "The place
// at a camera of the game"). A move of the game camera that comfort_camera.cpp found, kept until
// the next main camera call, which uses it once. again is the test key: the
// view goes to the game camera of now. The cause of the next build, for the
// log. The game camera and the version of the view at the last place, so the
// same camera does not place the view two times.
struct SpawnRequest {
  bool pending;
  bool again;
  char cause[96];
  float position[3];
  float focus[3];
};
SpawnRequest g_spawn = {};
// THE GAME CAMERA OF THE VIEW. The game camera that the view wrote
// (GodAfter), or that the view went to, with the view that it stands for.
// When the game camera is there again, for example after a story that puts
// the camera back, the view comes back exactly where it was. The pair is good
// only while the player did not move the view after it (version).
struct CameraPair {
  bool have;
  float position[3];
  float focus[3];
  TablePose pose;
  uint64_t version;
};
CameraPair g_pair = {};
const float kPairUnits = 5.0f;
const float kPairFocusShare = 0.05f;
const char* g_build_cause = "the first view of this land";
bool g_have_placed = false;
float g_placed_position[3] = {0.0f, 0.0f, 0.0f};
float g_placed_focus[3] = {0.0f, 0.0f, 0.0f};
uint64_t g_placed_version = 0;
uint64_t g_spawn_moves = 0;
uint64_t g_spawn_same = 0;
int g_spawn_logs = 0;
const int kMaxSpawnLogs = 80;

// THE MOUSE PAN (MousePan). The grab of the press, the head of the press,
// where the ray starts, and the serial that the anchor was made at. dead is
// a drag that pans nothing until the release.
struct MousePanState {
  bool active;
  bool dead;
  TableGrab anchor;
  float origin[3];
  float grabbed[3];  // the grabbed land point in the world
  uint64_t serial;
  uint64_t frames;
  uint64_t moved;
  float worst_cm;  // the grabbed point's largest distance from the ray
};
MousePanState g_pan = {};
uint64_t g_pans = 0;
uint64_t g_pan_moves = 0;
uint64_t g_pan_held = 0;
// The moves that a camera switch refused, and the loads that cleared the map.
uint64_t g_switch_refused = 0;
uint64_t g_clears = 0;

// THE WALK WITH THE STICK (ApplyGodWalk). push is what the stick asked for
// since the last main camera call, aim is the forward direction of its
// controller on the land in the room, and fresh says that the stick asked.
// The rest is the walk that runs: its rule, the forward direction of its
// last frame, the waist point, the land point under the head and the scale
// at its start, that land point after its last frame, the time of its
// frames, how many of them moved the view, and the way that the player went
// in the room. For the log of the end: the shortest and the longest frame
// and step, how far the view went up and down with the land, and how far
// the land under the head was below the head at the start.
struct WalkHold {
  float push[2] = {0.0f, 0.0f};
  bool fresh = false;
  bool have_aim = false;
  float aim[2] = {0.0f, 1.0f};
  bool have_aim3 = false;
  float aim3[3] = {0.0f, 0.0f, 1.0f};  // the fly: the controller in 3D
  bool active = false;
  int rule = 0;
  int type = 0;  // move_type at the start: the walk or the fly
  bool have_forward = false;
  float forward[2] = {0.0f, 1.0f};
  // The fly: forward in 3D and its level right, of the last frame.
  float forward3[3] = {0.0f, 0.0f, 1.0f};
  float right[2] = {1.0f, 0.0f};
  float climb = 0.0f;  // the fly: the sum of the steps up, meters of room
  // The speed ramp: the seconds of the full push, and the largest factor.
  float ramp_held = 0.0f;
  float ramp_top = 1.0f;
  // The speed by the scale: the least and the largest factor of the walk.
  float scale_least = 1.0f;
  float scale_most = 1.0f;
  // The fly: how far its floor lifted the view, in meters of the room, on
  // how many frames, the least clearance of the head after a step, and the
  // steps that stopped short of a solid.
  float lifted = 0.0f;
  uint64_t lift_frames = 0;
  float least_clear = 1000.0f;
  uint64_t solid_stops = 0;
  uint64_t solid_slides = 0;
  // The solids that the fly met: the last one and the way, for one log line
  // for each solid, and their count.
  void* solid_last = nullptr;
  int solid_way = 0;
  uint64_t solids_met = 0;
  float start_center[2] = {0.0f, 0.0f};  // the waist point, x and z
  float start_under[2] = {0.0f, 0.0f};
  float under[2] = {0.0f, 0.0f};
  float start_scale = 0.0f;
  float went[2] = {0.0f, 0.0f};  // the sum of the steps, meters of the room
  float path = 0.0f;             // the sum of their lengths
  float sunk = 0.0f;  // how far ZoomClearHead put the land down, in meters
  double seconds = 0.0;
  uint64_t frames = 0;
  uint64_t moved = 0;
  uint64_t blind = 0;  // frames in which the controller had no direction
  float frame_least = 0.0f, frame_most = 0.0f;  // seconds
  float step_least = 0.0f, step_most = 0.0f;    // meters of the room
  float rose = 0.0f, fell = 0.0f;  // the follow of the land, meters of room
  float start_below = 0.0f, below = 0.0f;  // meters of the room
  bool have_below = false;
  // The display times of the headset in the walk: the last one, how many
  // came after it, and how many of those came more than one refresh later.
  int64_t shown_at = 0, period = 0;
  uint64_t shown = 0, late = 0;
} g_walk;
// THE LAND THAT THE VIEW FOLLOWS (move_stick_follow_land=1, walk_math.h).
// The filter keeps its state from one walk to the next, so a climb in short
// pushes follows the land as a long push does. version and nonturn are
// g_god_version and g_god_nonturn_version after the last frame of the walk
// that moved the view, and origin is the count of the recenters then. When
// something else moved the view since then (a teleport, a grab, a zoom, a
// ground step, a fly, a place at a camera of the game, a recenter), the
// filter starts again at the land under the head (walk_math::FollowRestart,
// move_stick_follow_place). placed is set by PlaceGod.
struct WalkFollow {
  walk_math::LandFollow land;
  bool have = false;
  uint64_t version = 0;
  uint64_t nonturn = 0;
  uint64_t origin = 0;
  bool placed = false;
} g_follow;
// The log lines of a start of the filter after a place or a recenter.
int g_follow_logs = 0;
// The test move_stick_trace: the lines of the trace in this game.
uint64_t g_walk_traces = 0;
const uint64_t kMaxWalkTraces = 2000;
// The check after a walk: this many frames after the stop, the log compares
// the waist point and the render head with the ones at the stop.
const int kWalkCheckFrames = 30;
struct WalkCheck {
  int frames_left = 0;
  float center[2] = {0.0f, 0.0f};  // the waist point at the stop, x and z
  uint64_t version = 0;
  bool have_eye = false;
  float eye[3] = {0.0f, 0.0f, 0.0f};
} g_walk_check;
uint64_t g_walks = 0;
uint64_t g_walk_moves = 0;
double g_walk_seconds = 0.0;
int g_walk_logs = 0;
const int kMaxWalkLogs = 120;
void StopWalk(const char* cause, bool view_stays);

// The sea disc, built with the table.
float g_fan_positions[kFanVertices][3];
unsigned g_fan_indices[kFanSegments][3];
float g_fan_uvs[kFanVertices][2];
uint32_t g_fan_colors[kFanVertices];
bool g_sea_patched = false;

// A VALUE THE MODE CHANGES. It keeps the game's own value, and puts it back
// when the mode goes off. When the game writes a value of its own while the
// mode is on, for example a script or a level load, that value becomes the
// one to put back.
struct Patch {
  uintptr_t address;
  uint32_t wanted;  // the bits of the float or the int
  uint32_t original;
  uint32_t written;
  bool have;
  bool usable;
};

uint32_t Bits(float value) {
  uint32_t bits;
  memcpy(&bits, &value, sizeof(bits));
  return bits;
}

float FloatOf(uint32_t bits) {
  float value;
  memcpy(&value, &bits, sizeof(value));
  return value;
}

Patch g_fog = {kFog, 0, 0, 0, false, false};
Patch g_vanish = {kVanish, 0, 0, 0, false, false};
Patch g_size_cap = {kSizeCap, 0, 0, 0, false, false};
Patch g_detail_near = {kDetailNear, 0, 0, 0, false, false};
Patch g_detail_far = {kDetailFar, 0, 0, 0, false, false};
Patch g_texture_line = {kTextureLine, 0, 0, 0, false, false};
Patch g_reflection = {kReflection, 0, 0, 0, false, false};
Patch g_reach = {0, 0, 0, 0, false, true};
Patch* const g_patches[] = {&g_fog,         &g_vanish,       &g_size_cap,
                            &g_detail_near, &g_detail_far,   &g_texture_line,
                            &g_reflection};

void ApplyPatch(Patch& p, bool live) {
  if (!p.usable || p.address == 0) return;
  uint32_t* at = reinterpret_cast<uint32_t*>(p.address);
  if (live) {
    if (!p.have || *at != p.written) {
      p.original = *at;
      p.have = true;
    }
    *at = p.wanted;
    p.written = p.wanted;
  } else if (p.have) {
    if (*at == p.written) *at = p.original;
    p.have = false;
  }
}

// A DISTANCE THAT FOLLOWS THE SCALE, for the God view. The game's own value
// times the gain, and at most `most`. At the life size scale the gain is 1,
// and the game keeps its own value. The same rule as ApplyPatch: a value
// that the game writes while the patch holds becomes the one to put back.
void ApplyScaled(Patch& p, bool live, float gain, float most) {
  if (!p.usable || p.address == 0) return;
  uint32_t* at = reinterpret_cast<uint32_t*>(p.address);
  if (live && gain > 1.0f) {
    if (!p.have || *at != p.written) {
      p.original = *at;
      p.have = true;
    }
    // Never more than `most`, never less than the game's own value.
    p.wanted = Bits(cull_math::ScaledValue(FloatOf(p.original), gain, most));
    *at = p.wanted;
    p.written = p.wanted;
  } else if (p.have) {
    if (*at == p.written) *at = p.original;
    p.have = false;
  }
}

// The game's own map object distance: the value that the patch puts back,
// or the value in place while the patch holds none.
float VanishGame() {
  if (g_vanish.have) return FloatOf(g_vanish.original);
  if (!g_vanish.usable) return 0.0f;
  return *reinterpret_cast<const float*>(kVanish);
}

// For the report.
uint64_t g_live_frames = 0;
uint64_t g_frames = 0;
uint64_t g_blocks_cut = 0;
uint64_t g_blocks_kept = 0;
uint64_t g_sea_discs = 0;
uint64_t g_mist_hidden = 0;
uint64_t g_key_clears = 0;
uint64_t g_key_rect_total = 0;
uint64_t g_bracket_ends = 0;
uint64_t g_moves = 0;
bool g_said_hand = false;
bool g_said_ears = false;
int32_t g_most_objects = 0;
int32_t g_most_sorted = 0;
uint64_t g_objects_full = 0;
uint64_t g_sorted_full = 0;
int g_full_logs = 0;
bool g_said_depth = false;
bool g_said_clear_failed = false;
bool g_said_aside = false;

bool GameCameraPlain() {
  const uint8_t* const* game = reinterpret_cast<const uint8_t* const*>(kGame);
  if (IsBadReadPtr(game, sizeof(void*)) || *game == nullptr) return false;
  const int32_t* source =
      reinterpret_cast<const int32_t*>(*game + kGameCameraSource);
  if (IsBadReadPtr(source, sizeof(int32_t))) return false;
  return *source == 0;
}

uint8_t* Hand() {
  void* const* game = reinterpret_cast<void* const*>(kGame);
  if (IsBadReadPtr(game, sizeof(void*)) || *game == nullptr) return nullptr;
  const MyInterfaceFn my_interface =
      reinterpret_cast<MyInterfaceFn>(kMyInterface);
  uint8_t* face = static_cast<uint8_t*>(my_interface(*game, nullptr));
  if (face == nullptr || IsBadReadPtr(face + kFaceHand, sizeof(void*))) {
    return nullptr;
  }
  return *reinterpret_cast<uint8_t* const*>(face + kFaceHand);
}

// THE LAND OF THIS LEVEL, told apart by the block table. A new level changes
// the count, the pointers or the corners of the first and last blocks.
bool ReadSignature(uint32_t out[4]) {
  const int32_t count = *reinterpret_cast<const int32_t*>(kBlockCount);
  if (count < 2 || count > kMaxBlocks) return false;
  uint8_t* const* table = reinterpret_cast<uint8_t* const*>(kBlockTable);
  const uint8_t* first = table[1];
  const uint8_t* last = table[count - 1];
  if (first == nullptr || last == nullptr ||
      IsBadReadPtr(first + kBlockX, 8) || IsBadReadPtr(last + kBlockX, 8)) {
    return false;
  }
  out[0] = static_cast<uint32_t>(count);
  out[1] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(first));
  out[2] = *reinterpret_cast<const uint32_t*>(first + kBlockX) ^
           *reinterpret_cast<const uint32_t*>(last + kBlockZ);
  out[3] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(last));
  return true;
}

bool BlockCorner(const uint8_t* block, float* x0, float* z0) {
  if (block == nullptr || IsBadReadPtr(block + kBlockX, 8)) return false;
  *x0 = *reinterpret_cast<const float*>(block + kBlockX);
  *z0 = *reinterpret_cast<const float*>(block + kBlockZ);
  return *x0 >= 0.0f && *x0 < kMapSize && *z0 >= 0.0f && *z0 < kMapSize;
}

float Altitude(float x, float z) {
  const int32_t coords[3] = {static_cast<int32_t>(x * kMapScale),
                             static_cast<int32_t>(z * kMapScale), 0};
  return reinterpret_cast<GetAltitudeFn>(kGetAltitude)(coords);
}

float ClampMap(float v) {
  if (!(v >= 0.0f)) return 0.0f;
  if (v > kMapSize - 1.0f) return kMapSize - 1.0f;
  return v;
}

// The land height at a point, with the point kept on the map. The sea, and a
// result that is not a height, is 0.
float GroundAt(float x, float z) {
  const float y = Altitude(ClampMap(x), ClampMap(z));
  return (y >= 0.0f && y < 1000.0f) ? y : 0.0f;
}

// The head in the room now, in meters, from the last head sample: its lean
// from the recenter pose at the life size scale. The recenter pose when there
// is no sample.
void HeadRoom(float out[3]) {
  float head[9], lean[3];
  const float life = HeadPoseScale();
  if (HeadPoseCurrent(head, lean) && life > 0.0f) {
    for (int i = 0; i < 3; ++i) out[i] = lean[i] / life;
    return;
  }
  out[0] = out[1] = out[2] = 0.0f;
}

void BuildSea();

// MEASURE THE LAND: its middle, and the distance to its farthest cell. The
// game asks the land for the height of every cell point of every block, and
// a point above the sea is land. Once for each land, about 25000 to 50000
// calls.
bool FitLand() {
  uint32_t signature[4];
  if (!ReadSignature(signature)) return false;
  if (g_have_fit && memcmp(signature, g_signature, sizeof(signature)) == 0) {
    return true;
  }
  LARGE_INTEGER hz, start, end;
  QueryPerformanceFrequency(&hz);
  QueryPerformanceCounter(&start);

  g_have_fit = false;
  g_have_table = false;
  memset(g_block_land, 0, sizeof(g_block_land));
  const int count = static_cast<int>(signature[0]);
  uint8_t* const* table = reinterpret_cast<uint8_t* const*>(kBlockTable);
  float land_low[2] = {kMapSize, kMapSize}, land_high[2] = {0.0f, 0.0f};
  float block_low[2] = {kMapSize, kMapSize}, block_high[2] = {0.0f, 0.0f};
  int blocks = 0, cells = 0;
  for (int b = 1; b < count; ++b) {
    float x0, z0;
    if (!BlockCorner(table[b], &x0, &z0)) continue;
    ++blocks;
    if (x0 < block_low[0]) block_low[0] = x0;
    if (z0 < block_low[1]) block_low[1] = z0;
    if (x0 + kBlockSize > block_high[0]) block_high[0] = x0 + kBlockSize;
    if (z0 + kBlockSize > block_high[1]) block_high[1] = z0 + kBlockSize;
    bool any = false;
    for (int i = 0; i < kBlockCells; ++i) {
      for (int j = 0; j < kBlockCells; ++j) {
        const float x = x0 + i * kCellSize, z = z0 + j * kCellSize;
        if (!(Altitude(x, z) > 0.0f)) continue;
        any = true;
        ++cells;
        if (x < land_low[0]) land_low[0] = x;
        if (z < land_low[1]) land_low[1] = z;
        if (x > land_high[0]) land_high[0] = x;
        if (z > land_high[1]) land_high[1] = z;
      }
    }
    const int bx = static_cast<int>(x0 / kBlockSize);
    const int bz = static_cast<int>(z0 / kBlockSize);
    if (any && bx >= 0 && bx < kMapBlocks && bz >= 0 && bz < kMapBlocks) {
      g_block_land[bx][bz] = true;
    }
  }
  if (blocks == 0) return false;
  const bool land = cells > 0;
  const float* low = land ? land_low : block_low;
  const float* high = land ? land_high : block_high;
  float* center = g_fit_center;
  center[0] = 0.5f * (low[0] + high[0]);
  center[1] = 0.0f;
  center[2] = 0.5f * (low[1] + high[1]);
  if (g_settings.center_x >= 0) {
    center[0] = static_cast<float>(g_settings.center_x);
  }
  if (g_settings.center_z >= 0) {
    center[2] = static_cast<float>(g_settings.center_z);
  }

  // The farthest land point from the center, in a second pass over the
  // blocks with land.
  float farthest = 0.0f;
  for (int b = 1; b < count; ++b) {
    float x0, z0;
    if (!BlockCorner(table[b], &x0, &z0)) continue;
    const int bx = static_cast<int>(x0 / kBlockSize);
    const int bz = static_cast<int>(z0 / kBlockSize);
    if (land && !g_block_land[bx][bz]) continue;
    for (int i = 0; i <= kBlockCells; i += land ? 1 : kBlockCells) {
      for (int j = 0; j <= kBlockCells; j += land ? 1 : kBlockCells) {
        const float x = x0 + i * kCellSize, z = z0 + j * kCellSize;
        if (land && (i == kBlockCells || j == kBlockCells ||
                     !(Altitude(x, z) > 0.0f))) {
          continue;
        }
        const float dx = x - center[0], dz = z - center[2];
        const float d = sqrtf(dx * dx + dz * dz);
        if (d > farthest) farthest = d;
      }
    }
  }
  g_far_radius = farthest > kBlockSize ? farthest : kBlockSize;

  const float diameter = g_settings.diameter_cm * 0.01f;
  const float margin = g_settings.margin_pct * 0.01f;
  g_fit_scale = g_settings.units_per_meter > 0
                    ? static_cast<float>(g_settings.units_per_meter)
                    : FitScale(g_far_radius, diameter, margin);
  memcpy(g_signature, signature, sizeof(g_signature));
  g_have_fit = g_fit_scale > 0.0f;

  QueryPerformanceCounter(&end);
  const double ms = (end.QuadPart - start.QuadPart) * 1000.0 /
                    static_cast<double>(hz.QuadPart);
  if (land) {
    ModLog("tabletop     : the land has %d blocks and %d land cells, x %.0f "
           "to %.0f and z %.0f to %.0f, measured in %.1f ms", blocks, cells,
           land_low[0], land_high[0], land_low[1], land_high[1], ms);
  } else {
    ModLog("tabletop     : no land cell in %d blocks, so the blocks set the "
           "size, measured in %.1f ms", blocks, ms);
  }
  ModLog("tabletop     : the table center is at %.0f %.0f, and the farthest "
         "land is %.0f units from it", center[0], center[2], g_far_radius);
  ModLog("tabletop     : the table is %d cm across at %.0f units per meter, "
         "so its edge is %.0f units from the center", g_settings.diameter_cm,
         g_fit_scale, CutRadius(g_fit_scale, diameter));
  return g_have_fit;
}

// The table center in the room, in meters. In the God view it is right
// under the recenter pose, at the waist.
void TableRoom(float out[3]) {
  if (g_settings.god) {
    GodRoom(g_settings.god_down_cm * 0.01f, out);
    return;
  }
  out[0] = 0.0f;
  out[1] = -g_settings.down_cm * 0.01f;
  out[2] = g_settings.forward_cm * 0.01f;
}

TablePose Pose() {
  TablePose pose;
  for (int i = 0; i < 3; ++i) pose.center[i] = g_center[i];
  pose.heading = g_heading;
  pose.scale = g_scale;
  return pose;
}

// The limits of the scale in the God view: god_near_units_per_meter_x10 (1
// unit for each meter, where a villager is as high as the player), and
// god_far_units_per_meter.
float GodLowest() {
  return zoom_math::LowestScale(g_settings.god_least_tenths, HeadPoseScale());
}

float GodHighest() {
  const float most = static_cast<float>(g_settings.god_most_units);
  return most > GodLowest() ? most : GodLowest();
}

// EVERYTHING THAT FOLLOWS FROM C, PSI AND S: the basis, where the recenter
// pose is in the world, the edge and the sea disc.
void Derive() {
  Basis(g_heading, g_basis);
  float table_room[3];
  TableRoom(table_room);
  Origin(g_center, g_basis, g_scale, table_room, g_origin);
  if (g_settings.god) {
    // No edge and no disc. The synthetic camera looks 1 m of the room ahead.
    g_focus_distance = g_scale > 1.0f ? g_scale : 1.0f;
    g_radius = kNoEdge;
    g_god_last_scale = g_scale;
    return;
  }
  const float d[3] = {g_center[0] - g_origin[0], g_center[1] - g_origin[1],
                      g_center[2] - g_origin[2]};
  g_focus_distance = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  if (!(g_focus_distance > 1.0f)) g_focus_distance = 1.0f;
  g_radius = CutRadius(g_scale, g_settings.diameter_cm * 0.01f);
  BuildSea();
}

// THE TABLE IN THE WORLD, from the fit of the land and the heading of the
// game camera when the mode starts. A new land and Ctrl F11 build it again.
void BuildTable(float heading) {
  memcpy(g_center, g_fit_center, sizeof(g_center));
  g_scale = g_fit_scale;
  g_heading = heading;
  Derive();
  g_have_table = true;
  ++g_serial;
  ModLog("tabletop     : the table faces heading %.1f degrees, and your "
         "recenter pose is at %.0f %.0f %.0f in the world", heading * 57.29578f,
         g_origin[0], g_origin[1], g_origin[2]);
}

void BuildSea() {
  SeaFan(g_center, g_radius, g_fan_positions, g_fan_indices);
}

// THE LAND OF THIS LEVEL, for the God view. It needs no fit, only to know
// when the land is new, and then the view is built again.
bool GodLand() {
  uint32_t signature[4];
  if (!ReadSignature(signature)) return false;
  if (!g_have_fit || memcmp(signature, g_signature, sizeof(signature)) != 0) {
    memcpy(g_signature, signature, sizeof(g_signature));
    g_have_fit = true;
    if (g_have_table) g_build_cause = "a new land";
    g_pair.have = false;
    g_have_table = false;
    ModLog("god view     : a new land of %u blocks", signature[0]);
  }
  return true;
}

// The land point that a game camera looks at, kept on the map: where the
// line from the camera to its focus meets the land, or the focus put on the
// land (CameraLookPoint in table_math.h). The line goes in steps of 1 unit.
void LookPoint(const float position[3], const float focus[3], float out[3]) {
  const auto height = [](float x, float z) { return GroundAt(x, z); };
  CameraLookPoint(position, focus, 1.0f, height, out);
  out[0] = ClampMap(out[0]);
  out[2] = ClampMap(out[2]);
  out[1] = GroundAt(out[0], out[2]);
}

// THE GOD VIEW GOES TO A CAMERA OF THE GAME (docs/god-view.md, "The place at a
// camera of the game"):
// when the mode starts, after Ctrl F11, on a new land, after a load, after a
// story, and each time the game puts its own camera at a new place.
//
// god_spawn=1. The land point that the game camera looks at goes
// god_spawn_ahead_cm in front of the player's head, and the flat forward of
// the head gets the heading of the game camera (GodSpawn). So the player
// looks at what the game shows, from the side that the game shows it from.
// The land at that point gets the height in the room that the land under the
// head had, so the ground steps of the player stay. A hill under the head
// stays 10 cm below the head.
//
// god_spawn=0. The land point goes under the recenter pose, and the room
// forward gets the heading of the game camera.
//
// The scale is the one of the last God view, or god_units_per_meter at the
// first start. At the first start the land at the look point is at the
// waist. With use_pair, a game camera that is where the view put it brings
// the view back where it was (g_pair). False when the game camera looks
// straight down, and then nothing changes.
// THE FLOOR OF THE HEAD (docs/god-view.md, section 5.9). When the view goes
// to a place, and in a zoom, a walk and a grab, the land under the head stays
// kGroundNearest below a height of the head in the room. With head_blinder=1
// that height is the height of the head at the recenter pose (0 in the
// room), so a head that goes down does not push the land down: the land
// stays where it is, and the overlay of head_blinder.cpp covers the view of
// a head in the land. With head_blinder=0 it is the height of the head now.
float FloorHeadY(const float head[3]) {
  return HeadBlinderOn() ? 0.0f : head[1];
}

// A log line when the floor of the head moves the land down.
void LogFloor(const char* where, float down_m, const float head[3]) {
  static int said = 0;
  if (said >= 40 || !(down_m > 0.0005f)) return;
  ++said;
  ModLog("god view     : the floor of the head acts (%s): the land goes "
         "%.1f cm down in the room, so the land under your head stays %.0f cm "
         "below the %s. Your head is %.1f cm %s the recenter pose", where,
         down_m * 100.0f, kGroundNearest * 100.0f,
         HeadBlinderOn() ? "height of your head at the recenter pose"
                         : "head",
         fabsf(head[1]) * 100.0f, head[1] < 0.0f ? "below" : "above");
}

bool PlaceGod(const char* cause, const float position[3],
              const float focus[3], bool use_pair) {
  float camera_heading = 0.0f;
  if (!Heading(position, focus, &camera_heading)) return false;
  const bool follow = g_settings.god_spawn != 0;
  const bool had = g_god_last_scale > 0.0f && g_scale > 0.0f;
  const TablePose before = Pose();
  float room[3];
  TableRoom(room);

  // The head: its place in the room and its yaw. With god_spawn=0 it is the
  // recenter pose.
  float head[3] = {0.0f, 0.0f, 0.0f};
  float head_yaw = 0.0f;
  if (follow) {
    HeadRoom(head);
    float matrix[9], lean[3];
    if (!HeadPoseCurrent(matrix, lean) || !HeadYaw(matrix, &head_yaw)) {
      head_yaw = 0.0f;
    }
  }

  // The height in the room that the land at the look point gets.
  float look_room_y = room[1];
  if (had && follow) {
    float at[3];
    PoseWorld(before, room, head, at);
    look_room_y = GroundInRoom(before, room, GroundAt(at[0], at[2]));
    const float floor_head = FloorHeadY(head);
    if (look_room_y > floor_head - kGroundNearest) {
      LogFloor("the place of a game camera",
               look_room_y - (floor_head - kGroundNearest), head);
      look_room_y = floor_head - kGroundNearest;
    }
    if (look_room_y < floor_head - kGroundFarthest) {
      look_room_y = floor_head - kGroundFarthest;
    }
  } else if (had) {
    float lift = (GroundAt(g_center[0], g_center[2]) - g_center[1]) / g_scale;
    const float down = g_settings.god_down_cm * 0.01f;
    if (lift > down - kGroundNearest) lift = down - kGroundNearest;
    if (lift < down - kGroundFarthest) lift = down - kGroundFarthest;
    look_room_y = room[1] + lift;
  }

  float scale = g_god_last_scale > 0.0f
                    ? g_god_last_scale
                    : static_cast<float>(g_settings.god_units_per_meter);
  if (scale < GodLowest()) scale = GodLowest();
  if (scale > GodHighest()) scale = GodHighest();
  const float ahead = follow ? g_settings.god_spawn_ahead_cm * 0.01f : 0.0f;
  float look[3] = {ClampMap(focus[0]), 0.0f, ClampMap(focus[2])};
  look[1] = GroundAt(look[0], look[2]);
  if (follow) LookPoint(position, focus, look);
  TablePose to;
  // The game camera is where the view put it, or where the view went to, and
  // the player did not move the view after that. The view comes back exactly
  // where it was. A script that puts the camera back after its story gives
  // this case.
  // The limits are the ones of a small move in comfort_camera.cpp: the game
  // puts the focus on the land again after each write of the mod.
  bool back = false;
  if (use_pair && follow && had && g_pair.have &&
      g_pair.version == g_god_version) {
    float position_off = 0.0f, focus_off = 0.0f, distance = 0.0f;
    for (int i = 0; i < 3; ++i) {
      const float p = position[i] - g_pair.position[i];
      const float f = focus[i] - g_pair.focus[i];
      const float d = focus[i] - position[i];
      position_off += p * p;
      focus_off += f * f;
      distance += d * d;
    }
    float focus_limit = kPairFocusShare * sqrtf(distance);
    if (focus_limit < kPairUnits) focus_limit = kPairUnits;
    back = sqrtf(position_off) < kPairUnits && sqrtf(focus_off) < focus_limit;
  }
  if (back) {
    to = g_pair.pose;
  } else {
    GodSpawn(look, camera_heading, scale, room, head, head_yaw, ahead,
             look_room_y, &to);
    for (int i = 0; i < 3; i += 2) to.center[i] = ClampMap(to.center[i]);
    // A hill under the head must not come over the head.
    if (follow) {
      float at[3];
      PoseWorld(to, room, head, at);
      const float under = GroundInRoom(to, room, GroundAt(at[0], at[2]));
      const float top = FloorHeadY(head) - kGroundNearest;
      if (under > top) {
        LogFloor("a hill at the place of a game camera", under - top, head);
        to.center[1] += (under - top) * to.scale;
      }
    }
  }

  for (int i = 0; i < 3; ++i) g_center[i] = to.center[i];
  g_scale = to.scale;
  g_heading = to.heading;
  g_god_zoom = g_god_ground = g_god_turn = 0;
  StopZoomHold("the view went to a camera of the game", false);
  StopTurnHold("the view went to a camera of the game", false);
  g_tele.pending = false;
  g_pan = {};
  // The view is at a new place, and the version of the view does not change
  // here (g_pair and the game camera need it). So the filter of the land of
  // the walk learns it from this flag (move_stick_follow_place).
  g_follow.placed = true;
  Derive();
  g_have_table = true;
  ++g_serial;
  g_have_placed = true;
  for (int i = 0; i < 3; ++i) {
    g_placed_position[i] = position[i];
    g_placed_focus[i] = focus[i];
  }
  g_placed_version = g_god_version;
  if (!back) {
    g_pair.have = true;
    for (int i = 0; i < 3; ++i) {
      g_pair.position[i] = position[i];
      g_pair.focus[i] = focus[i];
    }
    g_pair.pose = Pose();
    g_pair.version = g_god_version;
  }

  if (g_spawn_logs < kMaxSpawnLogs) {
    ++g_spawn_logs;
    const float d[3] = {focus[0] - position[0], focus[1] - position[1],
                        focus[2] - position[2]};
    char was[128];
    if (had) {
      sprintf_s(was, "The waist point was at %.1f %.1f %.1f, heading %.1f "
                "degrees.", before.center[0], before.center[1],
                before.center[2], before.heading * 57.29578f);
    } else {
      sprintf_s(was, "The waist point had no place before.");
    }
    float at[3], look_room[3];
    PoseWorld(to, room, head, at);
    PoseRoom(to, room, look, look_room);
    const float dx = look_room[0] - head[0], dz = look_room[2] - head[2];
    ModLog("god view     : the view goes to the game camera (%s). The game "
           "camera is at %.1f %.1f %.1f and looks at %.1f %.1f %.1f, heading "
           "%.1f degrees, %.1f units from its focus. %s Now it is at %.1f "
           "%.1f %.1f, heading %.1f degrees, %.1f units per meter. The land "
           "at %.0f %.0f %.0f is %.0f cm in front of your head and %.2f m "
           "below it, and the land under your head is %.2f m below it "
           "(god_spawn=%d).%s", cause, position[0], position[1], position[2],
           focus[0], focus[1], focus[2], camera_heading * 57.29578f,
           sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]), was, g_center[0],
           g_center[1], g_center[2], g_heading * 57.29578f, g_scale, look[0],
           look[1], look[2], sqrtf(dx * dx + dz * dz) * 100.0f,
           head[1] - look_room[1],
           head[1] - GroundInRoom(to, room, GroundAt(at[0], at[2])),
           follow ? 1 : 0,
           back ? " The game camera is where the view put it, so the view "
                  "comes back where it was."
                : "");
  }
  return true;
}

// The game camera to build the view from. With god_spawn=1 it is where the
// camera goes (the destinations of its Zoomers), so a camera that still
// glides gives its place at once. Otherwise it is the camera of the main
// call.
void GameCameraNow(const float asked_position[3], const float asked_focus[3],
                   float position[3], float focus[3]) {
  if (g_settings.god_spawn != 0 &&
      ComfortCameraDestinations(position, focus)) {
    return;
  }
  for (int i = 0; i < 3; ++i) {
    position[i] = asked_position[i];
    focus[i] = asked_focus[i];
  }
}

// A MOVE OF THE GAME CAMERA, or the test key, at the main camera call: the
// view goes there as one cut. The same camera does not place the view again
// while the player did not move the view.
void ApplyGodSpawn(const float asked_position[3],
                   const float asked_focus[3]) {
  if (!g_spawn.pending && !g_spawn.again) return;
  const bool again = g_spawn.again;
  g_spawn.pending = false;
  g_spawn.again = false;
  float position[3], focus[3];
  const char* cause = g_spawn.cause;
  if (again) {
    GameCameraNow(asked_position, asked_focus, position, focus);
    cause = "the test key";
  } else {
    for (int i = 0; i < 3; ++i) {
      position[i] = g_spawn.position[i];
      focus[i] = g_spawn.focus[i];
    }
    float off = 0.0f;
    for (int i = 0; i < 3; ++i) {
      off += fabsf(position[i] - g_placed_position[i]) +
             fabsf(focus[i] - g_placed_focus[i]);
    }
    if (g_have_placed && off < 1.0f && g_placed_version == g_god_version) {
      ++g_spawn_same;
      if (g_spawn_logs < kMaxSpawnLogs) {
        ++g_spawn_logs;
        ModLog("god view     : the view is at that game camera already (%s), "
               "so it stays", cause);
      }
      return;
    }
  }
  if (!PlaceGod(cause, position, focus, !again)) return;
  ++g_spawn_moves;
  ComfortCameraBlink();
}

bool GodLog() {
  if (g_god_logs >= kMaxGodLogs) return false;
  ++g_god_logs;
  return true;
}

bool ZoomLog() {
  if (g_zoom_logs >= kMaxZoomLogs) return false;
  ++g_zoom_logs;
  return true;
}

uint32_t CameraFeatures() {
  return *reinterpret_cast<const uint32_t*>(kEnabledFeatures);
}

// One event to the camera help, as the game sends it.
void Help(int event, unsigned inputs) {
  reinterpret_cast<HelpFn>(kCameraHelpCallback)(event, g_center, inputs);
}

// The events of a move by the grab: a drag of the land when the center
// moved, a turn when the heading changed, and a zoom when the scale changed.
// A heading that goes up is a turn of the view to the right (GodTurn).
void MoveHelp(const TablePose& before, unsigned inputs) {
  const float dx = g_center[0] - before.center[0];
  const float dz = g_center[2] - before.center[2];
  if (dx * dx + dz * dz > 1e-6f) Help(kHelpDrag, inputs);
  const float turned = Wrap(g_heading - before.heading);
  if (fabsf(turned) > 1e-5f) {
    Help(kHelpTurn, inputs);
    Help(turned > 0.0f ? kHelpTurnRight : kHelpTurnLeft, inputs);
  }
  if (fabsf(g_scale - before.scale) > 1e-4f * before.scale) {
    Help(kHelpZoom, inputs);
  }
}

float ClampCenter(float v) {
  if (!(v >= 0.0f)) return 0.0f;
  if (v > kMapSize) return kMapSize;
  return v;
}

// THE MOUSE PAN (docs/god-view.md, "Moving around: the grab and the mouse"). A land drag of the mouse pans
// the God view. At the press, the ray of the pointer from the head finds the
// land (RayLand). That land point is the grabbed point, and its height in the
// room sets a level plane. On each frame the ray of the pointer meets that
// plane, and the pan of one hand puts the grabbed point there
// (TableGrabSolve). So the land point under the mouse at the press stays
// under the mouse.
//
// The ray keeps the head of the press, so a head that moves with a still
// mouse moves no land. The land moves only on a frame with a land drag, so
// nothing moves after the release. A step of the view during the drag, a
// zoom or a turn, anchors the pan again with no move. A drag with no land
// under the ray at the press, a drag of a controller, and a drag that the
// game's switch does not allow pan nothing until the release. The game's
// own drag does not move the game camera (comfort_camera.cpp, GodDragAfter),
// and it sends the tutorial's drag event (0x308) itself.
void MousePan(bool drag) {
  if (!drag) {
    if (g_pan.active && GodLog()) {
      ModLog("god view     : the mouse let go of the land. The pan moved the "
             "view on %llu of %llu frames, and the grabbed land stayed within "
             "%.3f cm of the mouse ray.", g_pan.moved, g_pan.frames,
             g_pan.worst_cm);
    }
    g_pan.active = false;
    g_pan.dead = false;
    return;
  }
  if (g_pan.dead) return;
  float dir[3];
  const bool mouse = PointerMapMouseRay(dir);
  const bool allowed = (CameraFeatures() & kFeatureDrag) != 0;
  if (!mouse || !allowed || ControllersGrabHeld()) {
    if (mouse && !allowed) {
      ++g_switch_refused;
      if (GodLog()) {
        ModLog("god view     : the game does not allow a drag of the land now "
               "(camera switch 8 is off), so the mouse pans nothing");
      }
    }
    g_pan.active = false;
    g_pan.dead = true;
    return;
  }
  float room[3];
  TableRoom(room);
  if (!g_pan.active || g_pan.serial != g_serial) {
    const bool again = g_pan.active;
    if (!again) HeadRoom(g_pan.origin);
    const TablePose pose = Pose();
    const auto height = [](float x, float z) { return GroundAt(x, z); };
    float hit[3];
    if (!RayLand(pose, room, g_pan.origin, dir, kPanStepUnits, kReach,
                 height, hit)) {
      g_pan.active = false;
      g_pan.dead = true;
      if (GodLog()) {
        ModLog("god view     : no land under the mouse within %.0f units, so "
               "this drag moves nothing", kReach);
      }
      return;
    }
    memset(&g_pan.anchor, 0, sizeof(g_pan.anchor));
    g_pan.anchor.hands = 1;
    g_pan.anchor.pose = pose;
    for (int i = 0; i < 3; ++i) g_pan.anchor.middle[i] = hit[i];
    PoseWorld(pose, room, hit, g_pan.grabbed);
    g_pan.active = true;
    g_pan.serial = g_serial;
    if (!again) {
      g_pan.frames = 0;
      g_pan.moved = 0;
      g_pan.worst_cm = 0.0f;
      ++g_pans;
    }
    if (GodLog()) {
      float world[3];
      PoseWorld(pose, room, hit, world);
      const float d[3] = {hit[0] - g_pan.origin[0], hit[1] - g_pan.origin[1],
                          hit[2] - g_pan.origin[2]};
      ModLog("god view     : the mouse %s the land at %.1f %.1f %.1f, %.2f m "
             "from your head", again ? "takes again" : "takes", world[0],
             world[1], world[2], sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]));
    }
    return;
  }
  ++g_pan.frames;
  float hand[3];
  if (!RayLevel(g_pan.origin, dir, g_pan.anchor.middle[1], kReach / g_scale,
                hand)) {
    // The ray goes over the plane, or meets it past the reach of the Hand.
    // The land waits where it is.
    ++g_pan_held;
    return;
  }
  const float zero[3] = {0.0f, 0.0f, 0.0f};
  float theta = 0.0f, k = 1.0f;
  TablePose next;
  TableGrabSolve(g_pan.anchor, room, hand, zero, false, false, 0.08f,
                 GodLowest(), GodHighest(), &theta, &k, &next);
  float x = ClampCenter(next.center[0]);
  float z = ClampCenter(next.center[2]);
  CameraZoneLimitMove(g_center, &x, &z, "the mouse pan");
  if (x == g_center[0] && z == g_center[2]) return;
  g_center[0] = x;
  g_center[2] = z;
  Derive();
  // The proof: how far the grabbed land is from the mouse ray now, in the
  // room. Near the edge of the map the clamp can hold the land back.
  {
    float at[3];
    PoseRoom(Pose(), room, g_pan.grabbed, at);
    const float d[3] = {at[0] - g_pan.origin[0], at[1] - g_pan.origin[1],
                        at[2] - g_pan.origin[2]};
    const float along = d[0] * dir[0] + d[1] * dir[1] + d[2] * dir[2];
    float off = 0.0f;
    for (int i = 0; i < 3; ++i) {
      const float part = d[i] - along * dir[i];
      off += part * part;
    }
    const float cm = sqrtf(off) * 100.0f;
    if (cm > g_pan.worst_cm) g_pan.worst_cm = cm;
  }
  ++g_serial;
  g_pan.serial = g_serial;
  ++g_god_version;
  g_god_nonturn_version = g_god_version;
  ++g_pan_moves;
  ++g_pan.moved;
}

// True for a point of a zoom that is not the land under the head.
bool ZoomPointAway(const ZoomPoint& point) {
  return point.kind == zoom_math::kPointLook ||
         point.kind == zoom_math::kPointLookFar ||
         point.kind == zoom_math::kPointHand;
}

// THE POINT OF A ZOOM, for a zoom step and for the smooth zoom
// (docs/god-view.md, "The zoom"). A zoom keeps this point of the world at its
// place in the room. This is the one place that chooses the point.
//
// ITS HEIGHT is the level of the zoom: the height of the land under the
// head. So the land of that height keeps its height in the room, and the
// ground stays where the player put it. The level goes into the range of a
// step of the ground first, so a hill that stands over the head goes down as
// the view zooms out. With god_zoom_far_level=1 a zoom in (direction 1) over
// a land more than 2.5 m below the head takes the land itself as the level
// (zoom_math::ZoomLevelFor), so the zoom in does not move that land farther
// down. A zoom out keeps the level in the range, so a deep land comes up
// toward it.
//
// ITS PLACE ACROSS is the choice of god_zoom_about (zoom_math.h,
// PointAcross). 0: under the head, so the player keeps their place in the
// world. 1: the land point that the head looks at, so the thing in the
// middle of the view stays there and only its size changes. The line of the
// look is the forward direction of the head, the land test is the one of the
// mouse pan (RayLand), and the point is at most god_zoom_about_far_cm from
// the head. 2: the Hand's point. With 1 and 2 the head moves over the land
// in a zoom, and the game camera follows it (TabletopGodGameCamera). A hill
// at the new place of the head stays below the head (ZoomClearHead).
void GodZoomPoint(const TablePose& pose, const float room[3],
                  const float head[3], int direction, ZoomPoint* out) {
  float at[3];
  PoseWorld(pose, room, head, at);
  const float land = GroundAt(at[0], at[2]);
  const float in_room = zoom_math::ZoomLevelFor(
      GroundInRoom(pose, room, land), head[1], kGroundNearest,
      kGroundFarthest, direction, g_settings.god_zoom_far_level != 0);
  out->pivot[0] = at[0];
  out->pivot[1] = pose.center[1] + (in_room - room[1]) * pose.scale;
  out->pivot[2] = at[2];
  out->below = head[1] - in_room;

  float matrix[9], lean[3], forward[3], hand_world[3], hand[3], across[3];
  const bool have_forward = HeadPoseCurrent(matrix, lean) &&
                            zoom_math::HeadForward(matrix, forward);
  // The Hand's point is a world point of the frame before. The view of that
  // frame gives its place in the room.
  const bool have_hand =
      g_settings.god_zoom_about == zoom_math::kAboutHand &&
      HandDirectRealPoint(hand_world);
  if (have_hand) PoseRoom(Pose(), room, hand_world, hand);
  // The line of the look goes in steps of 1 cm of the room, and of the step
  // of the mouse pan at least.
  float step = 0.01f * pose.scale;
  if (step < kPanStepUnits) step = kPanStepUnits;
  const auto height = [](float x, float z) { return GroundAt(x, z); };
  out->kind = zoom_math::PointAcross(
      g_settings.god_zoom_about, pose, room, head,
      have_forward ? forward : nullptr, have_hand ? hand : nullptr,
      g_settings.god_zoom_about_far_cm * 0.01f, step, height, across);
  out->across = zoom_math::Across(head, across);
  if (ZoomPointAway(*out)) {
    zoom_math::PivotAt(pose, room, across, in_room, out->pivot);
  }
}

// Which point a zoom got and why, for the log.
void ZoomPointText(const ZoomPoint& point, char* out, size_t size) {
  const int about = g_settings.god_zoom_about;
  const float far_m = g_settings.god_zoom_about_far_cm * 0.01f;
  switch (point.kind) {
    case zoom_math::kPointLook:
      sprintf_s(out, size, "the land that you look at, %.2f m across from "
                "your head (god_zoom_about=%d)", point.across, about);
      break;
    case zoom_math::kPointLookFar:
      sprintf_s(out, size, "the line of your look meets no land within %.2f "
                "m, so the point of that line at %.2f m, %.2f m across from "
                "your head (god_zoom_about=%d)", far_m, far_m, point.across,
                about);
      break;
    case zoom_math::kPointLookNone:
      sprintf_s(out, size, "your head has no direction, so the land under "
                "your head (god_zoom_about=%d)", about);
      break;
    case zoom_math::kPointHand:
      sprintf_s(out, size, "the Hand's point, %.2f m across from your head "
                "(god_zoom_about=%d)", point.across, about);
      break;
    case zoom_math::kPointHandNone:
      sprintf_s(out, size, "no controller places the Hand, so the land under "
                "your head (god_zoom_about=%d)", about);
      break;
    default:
      sprintf_s(out, size, "the land under your head (god_zoom_about=%d)",
                about);
      break;
  }
}

// How far a world point moved in the room between two views, in cm.
float MovedInRoomCm(const TablePose& from, const TablePose& to,
                    const float room[3], const float world[3]) {
  float a[3], b[3];
  PoseRoom(from, room, world, a);
  PoseRoom(to, room, world, b);
  const float d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
  return sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) * 100.0f;
}

// The land height under the head in a view, with the waist point kept on
// the map.
float LandUnderHead(const TablePose& pose, const float room[3],
                    const float head[3]) {
  TablePose on_map = pose;
  for (int i = 0; i < 3; i += 2) on_map.center[i] = ClampMap(pose.center[i]);
  float at[3];
  PoseWorld(on_map, room, head, at);
  return GroundAt(at[0], at[2]);
}

// A HILL UNDER THE HEAD AFTER A ZOOM. A zoom about a point that is not under
// the head moves the head over the land, as a pan does. When the land at the
// new place of the head is nearer than kGroundNearest below the head, all of
// the land goes down to that limit (zoom_math.h, ClearHead), as it does when
// the view goes to a place (PlaceGod). pose is the view that the zoom gave.
// Returns how far the land went down, in meters of the room.
float ZoomClearHead(TablePose* pose, const float room[3],
                    const float head[3]) {
  const float y = zoom_math::ClearHead(
      pose->center[1], LandUnderHead(*pose, room, head), room[1], pose->scale,
      FloorHeadY(head), kGroundNearest);
  const float down = (y - pose->center[1]) / pose->scale;
  pose->center[1] = y;
  LogFloor("a zoom or a walk", down, head);
  return down;
}

// THE SOLIDS IN THE WAY OF A ZOOM (god_zoom_solid_stop). With
// god_zoom_about 1 and 2 a zoom step moves the head over the land, and with
// each value a step in brings the head nearer to the land and to what stands
// on it. Near the lowest scale a step can then put the head into a house. So
// a step stops where the head is god_zoom_solid_cm (30) out of the nearest
// solid, or it is refused when it cannot go at all. 30 cm leaves room for
// the Hand between the eyes and the wall, and the eyes then see the wall
// with the land beside it. The overlay of the head blinder starts at 5 cm,
// so the stop is well before it. A value of 5 stops the step where the
// overlay starts. The solids are the ones near the hands and the head that
// the first eye pass of the frame before kept (hand_direct.cpp,
// LatchSolids). Near the lowest scale the head wants them from
// kZoomSolidReachM of the room (TabletopZoomSolidReachM), so a house that a
// step brings to the head is in that list.
float SolidClearM() { return g_settings.god_zoom_solid_cm * 0.01f; }
const float kZoomSolidReachM = 1.5f;

struct SolidStop {
  bool short_step;   // the step stops short or is refused
  float full_cm;     // the clearance of the whole step, in cm of the room
  void* object;      // the solid of the whole step
};

// The clearance of the head from the nearest solid, in meters of the room, at
// a pose. which gets the index of that solid.
float HeadSolidClearance(const TablePose& pose, const float room[3],
                         const float head[3], const solid_math::Solid* solids,
                         int count, int* which) {
  float at[3];
  PoseWorld(pose, room, head, at);
  const float least = solid_math::Least(solids, count, at, which);
  return pose.scale > 0.0f ? least / pose.scale : 1e30f;
}

// The ratio of a zoom about pivot that keeps the head out of the solids: the
// ratio itself, a ratio nearer to 1, or exactly 1 for a step that cannot go.
float ZoomSolidRatio(const TablePose& pose, const float room[3],
                     const float head[3], const float pivot[3], float ratio,
                     SolidStop* stop) {
  stop->short_step = false;
  stop->full_cm = 0.0f;
  stop->object = nullptr;
  if (g_settings.god_zoom_solid_stop == 0 || !zoom_math::MayStop(ratio)) {
    return ratio;
  }
  static void* objects[solid_math::kMostSolids];
  static solid_math::Solid solids[solid_math::kMostSolids];
  const int count =
      HandDirectSolids(objects, solids, solid_math::kMostSolids);
  if (count == 0) return ratio;
  const float low = GodLowest(), high = GodHighest();
  const auto clear = [&](float t) {
    TablePose next;
    GodZoomAbout(pose, pivot, powf(ratio, t), low, high, &next);
    int which = -1;
    return HeadSolidClearance(next, room, head, solids, count, &which);
  };
  const float share = zoom_math::StepShare(clear, SolidClearM());
  if (share >= 1.0f) return ratio;
  TablePose full;
  GodZoomAbout(pose, pivot, ratio, low, high, &full);
  int which = -1;
  stop->full_cm =
      HeadSolidClearance(full, room, head, solids, count, &which) * 100.0f;
  stop->object = which >= 0 ? objects[which] : nullptr;
  stop->short_step = true;
  const float used = powf(ratio, share);
  return fabsf(used - 1.0f) < 1e-4f ? 1.0f : used;
}

// THE CLEARANCE OF THE HEAD FOR THE FLOOR OF A FLY, in meters of the room,
// at a pose: the measure of the head blinder (blinder_math::Clearance), the
// head point over the highest land at it and at four points 6 cm of the
// room around it. head is the head in the room now.
float FlyLandClearance(const TablePose& pose, const float room[3],
                       const float head[3]) {
  if (!(pose.scale > 0.0f)) return 1000.0f;
  float at[3];
  PoseWorld(pose, room, head, at);
  float points[blinder_math::kPoints][2];
  blinder_math::AroundPoints(at[0], at[2], pose.scale, points);
  float ground[blinder_math::kPoints];
  for (int i = 0; i < blinder_math::kPoints; ++i) {
    ground[i] = GroundAt(points[i][0], points[i][1]);
  }
  return blinder_math::Clearance(at[1], ground, blinder_math::kPoints,
                                 pose.scale);
}

bool WalkLog();

// One log line for each solid that the fly slides past or stops at: when
// the solid or the way changes.
void SayFlySolid(void* object, int way, float full_cm, float clear_cm,
                 const float middle[3], const float head[3],
                 const TablePose& pose) {
  if (way == walk_math::kSlideStop) {
    ++g_walk.solid_stops;
  } else {
    ++g_walk.solid_slides;
  }
  if (object == g_walk.solid_last && way == g_walk.solid_way) return;
  if (object != g_walk.solid_last) ++g_walk.solids_met;
  g_walk.solid_last = object;
  g_walk.solid_way = way;
  if (!WalkLog()) return;
  // The direction of the solid from the head, in the room, for a recipe
  // that flies straight at it.
  float room_dir[2];
  walk_math::InRoom(pose, middle[0] - head[0], middle[2] - head[2], room_dir);
  const float right = walk_math::DegreesRight(room_dir);
  ModLog("god view     : the fly %s the %s %p at %.1f %.1f %.1f: the whole "
         "step puts your head %.1f cm %s it, and your head stays %.1f cm out "
         "of it (the shell is %.0f cm, fly_solid_slide=%d). Your head is at "
         "%.1f %.1f %.1f, and the middle of the solid is %.1f degrees to the "
         "%s of straight ahead in the room, frame %llu",
         way == walk_math::kSlideStop ? "stops at" : "slides past",
         object != nullptr ? HandDirectSolidClass(object) : "solid", object,
         middle[0], middle[1], middle[2], fabsf(full_cm),
         full_cm < 0.0f ? "into" : "out of", clear_cm, SolidClearM() * 100.0f,
         g_settings.fly_solid_slide, head[0], head[1], head[2], fabsf(right),
         right < -0.05f ? "left" : "right", ControllersFrameCount());
}

// THE SOLIDS IN THE WAY OF A FLY (god_zoom_solid_stop=1), with the solids
// of HandDirectSolids, as for a zoom step. The fly asks for the solids
// within kZoomSolidReachM of the head (TabletopZoomSolidReachM). With
// fly_solid_slide=1 the step slides past a solid: only the part that goes
// into its shell of god_zoom_solid_cm comes off, and the part along it
// stays (walk_math::FlySlide). So a push at a slant goes round a tree and a
// push up at a wall goes over the house, and only a push straight at a
// solid stops. With 0 the step stops short of the shell. The step of the
// head in the world is the step of the waist point: the heading and the
// scale stay.
void FlySolidStop(const TablePose& from, const float room[3],
                  const float head[3], TablePose* next) {
  if (g_settings.god_zoom_solid_stop == 0 || !(from.scale > 0.0f)) return;
  static void* objects[solid_math::kMostSolids];
  static solid_math::Solid solids[solid_math::kMostSolids];
  const int count =
      HandDirectSolids(objects, solids, solid_math::kMostSolids);
  if (count == 0) return;
  float p[3];
  PoseWorld(from, room, head, p);
  const float step[3] = {next->center[0] - from.center[0],
                         next->center[1] - from.center[1],
                         next->center[2] - from.center[2]};
  const auto dist = [&](const float q[3]) {
    int which = -1;
    return solid_math::Least(solids, count, q, &which);
  };
  float out[3];
  const int way = walk_math::FlySlide(
      dist, p, step, SolidClearM() * from.scale, 0.001f * from.scale,
      g_settings.fly_solid_slide != 0, out);
  if (way == walk_math::kSlideFree) return;
  const float q[3] = {p[0] + step[0], p[1] + step[1], p[2] + step[2]};
  int which = -1;
  const float full_cm =
      solid_math::Least(solids, count, q, &which) / from.scale * 100.0f;
  void* object = which >= 0 ? objects[which] : nullptr;
  const float none[3] = {0.0f, 0.0f, 0.0f};
  const float* middle = which >= 0 ? solids[which].middle : none;
  for (int i = 0; i < 3; ++i) next->center[i] = from.center[i] + out[i];
  const float r[3] = {p[0] + out[0], p[1] + out[1], p[2] + out[2]};
  int near_one = -1;
  const float clear_cm =
      solid_math::Least(solids, count, r, &near_one) / from.scale * 100.0f;
  SayFlySolid(object, way, full_cm, clear_cm, middle, p, from);
}

void SayZoomSolid(const SolidStop& stop, bool refused, float from, float to,
                  const char* by) {
  if (!ZoomLog()) return;
  const char* name =
      stop.object != nullptr ? HandDirectSolidClass(stop.object) : "solid";
  if (refused) {
    ModLog("god view     : %s is refused at %.2f units per meter: it puts your "
           "head %.1f cm %s the %s %p (god_zoom_solid_stop=1)", by, from,
           fabsf(stop.full_cm), stop.full_cm < 0.0f ? "into" : "out of", name,
           stop.object);
  } else {
    ModLog("god view     : %s stops short at %.2f units per meter, not %.2f: "
           "the whole step puts your head %.1f cm %s the %s %p, so your head "
           "stays %.0f cm out of it (god_zoom_solid_stop=1)", by, to, from,
           fabsf(stop.full_cm), stop.full_cm < 0.0f ? "into" : "out of", name,
           stop.object, SolidClearM() * 100.0f);
  }
}

// THE LOWEST SCALE. One log line each time a zoom in comes to the lowest
// scale (god_near_units_per_meter_x10): the size of a villager in the room,
// and the land under the head. Below the life size scale the land stays at
// the height that the player gave it, so a player who wants the eyes of a
// villager lowers the land with the grip of the other hand. The line comes
// again only after the scale went above the lowest scale.
bool g_said_lowest = false;

void SayLowest(const TablePose& pose, const float room[3],
               const float head[3]) {
  const float lowest = GodLowest();
  if (!(pose.scale <= lowest * 1.0001f)) {
    g_said_lowest = false;
    return;
  }
  if (g_said_lowest || !ZoomLog()) return;
  g_said_lowest = true;
  float at[3];
  PoseWorld(pose, room, head, at);
  const float under = head[1] - GroundInRoom(pose, room, GroundAt(at[0], at[2]));
  ModLog("god view     : the zoom is at its lowest scale, %.2f units per meter "
         "(god_near_units_per_meter_x10=%d). A villager of %.1f units is "
         "%.2f m high in the room, and the land under your head is %.2f m "
         "below it", lowest, g_settings.god_least_tenths,
         zoom_math::kVillagerUnits, zoom_math::kVillagerUnits / pose.scale,
         under);
}

// THE STEPS OF THE GOD VIEW, once a frame, at the main camera call: a snap
// turn, a zoom step and a step of the ground, each at most once. Each one
// is an instant step, never a glide. A step moves the land under the player
// only, so the spirits stay where they float in the room, and a grab anchors
// again at its next solve.
void ApplyGodSteps() {
  if (g_god_zoom == 0 && g_god_ground == 0 && g_god_turn == 0 &&
      !g_tele.pending) {
    return;
  }
  const int zoom = g_god_zoom, ground = g_god_ground, turn = g_god_turn;
  g_god_zoom = g_god_ground = g_god_turn = 0;
  const bool teleport = g_tele.pending;
  const float tele_target[3] = {g_tele.target[0], 0.0f, g_tele.target[2]};
  g_tele.pending = false;
  float room[3], head[3];
  TableRoom(room);
  HeadRoom(head);
  TablePose pose = Pose();
  TablePose next = pose;
  bool moved = false;
  bool placed = false;  // a step that is not a snap turn

  if (turn != 0) {
    GodTurn(pose, room, head, turn * g_god_turn_degrees / 57.29578f, &next);
    pose = next;
    moved = true;
    if (GodLog()) {
      ModLog("god view     : turned %s by %.0f degrees about your head, heading "
             "now %.1f degrees", turn > 0 ? "right" : "left",
             g_god_turn_degrees, pose.heading * 57.29578f);
    }
  }

  if (zoom != 0) {
    // The point of the zoom keeps its place in the room (GodZoomPoint). It
    // is taken one time for the step.
    float at[3];
    ZoomPoint point;
    PoseWorld(pose, room, head, at);
    GodZoomPoint(pose, room, head, zoom > 0 ? 1 : -1, &point);
    const float step = 1.0f + g_settings.god_zoom_pct * 0.01f;
    const float wanted = zoom > 0 ? 1.0f / step : step;
    const float before = pose.scale;
    SolidStop stop;
    const float ratio =
        ZoomSolidRatio(pose, room, head, point.pivot, wanted, &stop);
    const float used = ratio == 1.0f
                           ? 1.0f
                           : GodZoomAbout(pose, point.pivot, ratio,
                                          GodLowest(), GodHighest(), &next);
    if (stop.short_step) {
      float full = before * wanted;
      if (full < GodLowest()) full = GodLowest();
      if (full > GodHighest()) full = GodHighest();
      SayZoomSolid(stop, used == 1.0f, used == 1.0f ? before : full,
                   next.scale, "the zoom step");
    }
    if (used != 1.0f) {
      // The head moves over the land, and a hill there stays below it.
      const float sunk =
          ZoomPointAway(point) ? ZoomClearHead(&next, room, head) : 0.0f;
      if (ZoomLog()) {
        char why[224], hill[128] = "";
        ZoomPointText(point, why, sizeof(why));
        if (sunk > 0.0005f) {
          sprintf_s(hill, ". The land went %.1f cm down in the room, so the "
                    "land under your head is %.0f cm below it",
                    sunk * 100.0f, kGroundNearest * 100.0f);
        }
        ModLog("god view     : zoomed %s, %.1f to %.1f units per meter, about "
               "the point at %.1f %.1f %.1f, %.2f m below your head: %s%s",
               zoom > 0 ? "in" : "out", before, next.scale, point.pivot[0],
               point.pivot[1], point.pivot[2], point.below, why, hill);
      }
      if (ZoomLog()) {
        // The view that the step gives, with the center kept on the map.
        TablePose to = next;
        for (int i = 0; i < 3; i += 2) to.center[i] = ClampMap(to.center[i]);
        float after[3];
        PoseWorld(to, room, head, after);
        const float gone = sqrtf((after[0] - at[0]) * (after[0] - at[0]) +
                                 (after[2] - at[2]) * (after[2] - at[2]));
        const float place = sqrtf((to.center[0] - pose.center[0]) *
                                      (to.center[0] - pose.center[0]) +
                                  (to.center[2] - pose.center[2]) *
                                      (to.center[2] - pose.center[2]));
        ModLog("god view     : zoom check, the point of the zoom moved %.2f cm "
               "in the room. The land under your head was at %.1f %.1f and "
               "is at %.1f %.1f, %.2f cm of the room apart. The recenter "
               "place moves %.2f cm of the room, and the head is %.0f cm "
               "right and %.0f cm forward of it",
               MovedInRoomCm(pose, to, room, point.pivot), at[0], at[2],
               after[0], after[2], gone / to.scale * 100.0f,
               place / to.scale * 100.0f, head[0] * 100.0f, head[2] * 100.0f);
      }
      g_zoom_probe.armed = HeadCameraTableEye(g_zoom_probe.eye);
      g_zoom_probe.before = before;
      g_zoom_probe.scale = next.scale;
      for (int i = 0; i < 3; ++i) g_zoom_probe.pivot[i] = point.pivot[i];
      pose = next;
      moved = true;
      placed = true;
      SayLowest(next, room, head);
      Help(kHelpZoom, kInputWheel);
    } else {
      ++g_god_refused;
      if (!stop.short_step && ZoomLog()) {
        ModLog("god view     : the zoom is at its limit, %.1f units per meter",
               pose.scale);
      }
    }
  }

  if (ground != 0) {
    const float rise = ground * g_settings.god_step_cm * 0.01f;
    GodRaise(pose, rise, &next);
    float at[3];
    PoseWorld(pose, room, head, at);
    const float land = GroundAt(at[0], at[2]);
    const float under = GroundInRoom(next, room, land);
    const bool too_high = rise > 0.0f && under > head[1] - kGroundNearest;
    const bool too_low = rise < 0.0f && under < head[1] - kGroundFarthest;
    if (too_high || too_low) {
      ++g_god_refused;
      if (GodLog()) {
        ModLog("god view     : the ground does not go %s: the land under your "
               "head would be %.2f m below it", rise > 0.0f ? "up" : "down",
               head[1] - under);
      }
    } else {
      pose = next;
      moved = true;
      placed = true;
      if (GodLog()) {
        ModLog("god view     : the ground went %s %d cm. The land under your "
               "head is %.2f m below it.", rise > 0.0f ? "up" : "down",
               g_settings.god_step_cm, head[1] - under);
      }
    }
  }

  // THE TELEPORT, one cut. The head goes over the target, and the land under
  // the head keeps its height in the room (GodTeleport).
  float head_before[3] = {0.0f, 0.0f, 0.0f};
  float under_before = 0.0f;
  if (teleport) {
    PoseWorld(pose, room, head, head_before);
    const float ground_head = GroundAt(head_before[0], head_before[2]);
    under_before = head[1] - GroundInRoom(pose, room, ground_head);
    TablePose to;
    GodTeleport(pose, room, head, ground_head, tele_target,
                GroundAt(tele_target[0], tele_target[2]), kGroundNearest,
                kGroundFarthest, &to);
    pose = to;
    moved = true;
    placed = true;
  }

  if (!moved) return;
  const TablePose before_all = Pose();
  // A zoom and a teleport keep to the camera zone of the game. A snap turn
  // alone does not: it turns about the head and does not move the player.
  float place_x = ClampMap(pose.center[0]);
  float place_z = ClampMap(pose.center[2]);
  if (placed) {
    CameraZoneLimitMove(g_center, &place_x, &place_z,
                        teleport ? "the teleport" : "a zoom step");
  }
  g_center[0] = place_x;
  g_center[2] = place_z;
  g_center[1] = pose.center[1];
  g_heading = Wrap(pose.heading);
  g_scale = pose.scale;
  Derive();
  ++g_serial;
  ++g_god_steps;
  ++g_god_version;
  if (placed) g_god_nonturn_version = g_god_version;
  ComfortCameraBlink();
  VignetteStep(teleport    ? "a teleport"
               : zoom != 0 ? "a zoom step"
               : turn != 0 ? "a snap turn"
                           : "a step of the ground");
  if (teleport) {
    float head_after[3];
    PoseWorld(Pose(), room, head, head_after);
    const float ground_after = GroundAt(head_after[0], head_after[2]);
    const float under_after = head[1] - GroundInRoom(Pose(), room, ground_after);
    const float dx = head_after[0] - tele_target[0];
    const float dz = head_after[2] - tele_target[2];
    ++g_tele_moves;
    ModLog("teleport     : the God view moved. The head went from %.2f %.2f to "
           "%.2f %.2f, and the target was %.2f %.2f, so it is %.3f units "
           "off. The land under the head was %.3f m below it, and now it is "
           "%.3f m. Scale %.2f to %.2f, heading %.2f to %.2f degrees, "
           "version %llu.", head_before[0], head_before[2], head_after[0],
           head_after[2], tele_target[0], tele_target[2],
           sqrtf(dx * dx + dz * dz), under_before, under_after,
           before_all.scale, g_scale, before_all.heading * 57.29578f,
           g_heading * 57.29578f, g_god_version);
    Help(TeleportJumpEvent(), kInputButton);
  }
}

// THE COMFORT VIGNETTE (vignette.cpp): the share of the full speed of a
// control that the view moved from one pose to the next in seconds: how far
// the land under the head went across the room, the turn and the zoom
// (comfort_math.h, MoveShare). With `height` the move up and down counts
// too, for the fly with the stick.
float MotionShare(const TablePose& from, const TablePose& to, double seconds,
                  bool height = false) {
  if (!(from.scale > 0.0f) || !(to.scale > 0.0f)) return 0.0f;
  float room[3], head[3], a[3], b[3];
  TableRoom(room);
  HeadRoom(head);
  PoseWorld(from, room, head, a);
  PoseWorld(to, room, head, b);
  const float dx = b[0] - a[0], dz = b[2] - a[2];
  const float dy = height ? b[1] - a[1] : 0.0f;
  const float meters =
      sqrtf(dx * dx + dy * dy + dz * dz) * 2.0f / (from.scale + to.scale);
  return comfort_math::MoveShare(meters,
                                 Wrap(to.heading - from.heading) * 57.29578f,
                                 to.scale / from.scale,
                                 static_cast<float>(seconds));
}

// THE END OF A HOLD of the smooth turn: the stick or the turn key came back,
// or the view cannot turn. A request that waits is dropped. One log line
// gives the degrees and the time of the frames of the hold, and the heading
// at the start and at the end. view_stays: the check after the hold starts,
// so the log shows that the view does not turn on after the stop.
void StopTurnHold(const char* cause, bool view_stays) {
  g_turn_hold.fresh = false;
  g_turn_hold.share = 0.0f;
  if (!g_turn_hold.active) return;
  g_turn_hold.active = false;
  g_turn_hold.check_frames = view_stays ? kHoldCheckFrames : 0;
  g_turn_hold.check_heading = g_heading;
  g_turn_hold.check_version = g_god_version;
  if (!GodLog()) return;
  const double rate = g_turn_hold.seconds > 0.0
                          ? g_turn_hold.degrees / g_turn_hold.seconds
                          : 0.0;
  ModLog("god view     : the smooth turn stops, %s. It turned %.2f degrees %s "
         "in %.3f s of %llu frames (%.1f degrees a second), heading %.2f to "
         "%.2f degrees. The clock of the CPU gave %.3f s for these "
         "frames, frame %llu", cause, fabsf(g_turn_hold.degrees),
         g_turn_hold.direction > 0 ? "right" : "left", g_turn_hold.seconds,
         g_turn_hold.frames, fabs(rate),
         g_turn_hold.start_heading * 57.29578f, g_heading * 57.29578f,
         g_turn_hold.cpu, ControllersFrameCount());
}

// THE SMOOTH TURN (snap_turn=4, docs/god-view.md, section 4). While the
// stick of the pointing hand is held to the side, or a turn key is held, the
// view turns about the head as a snap turn does (GodTurn), by a small angle
// in each frame: the share of the push times smooth_turn_speed times the
// time of the frame, with the cap of the smooth zoom
// (comfort_math::SmoothTurnDegrees).
//
// THE TIME OF A FRAME is the time of the walk and the smooth zoom
// (frame_seconds), latched once for the frame: the time between the display
// times of two frames of the headset (god_display_time=1), and the time of
// the CPU between two main camera calls with no session or with
// god_display_time=0. The stop line of a hold also gives the time of the
// CPU for the same frames (cpu_seconds). comfort_camera.cpp gives the request
// with the tests of a snap turn (TabletopGodTurnHold). A frame with no
// request turns nothing, so the turn stops in the frame in which the stick
// comes back, and nothing glides.
//
// A turn alone does not raise g_god_nonturn_version, so the game camera
// turns about its own focus, as for a snap turn (god_turn_keeps_focus,
// TabletopGodTurnedOnly). The version of the view goes up on each frame that
// turns, as for a walk, so the game camera follows the view. The turn keeps
// to no camera zone of the game: it turns about the head and does not move
// the player.
void ApplyGodTurnHold(double frame_seconds, double cpu_seconds) {
  const float share = g_turn_hold.fresh ? g_turn_hold.share : 0.0f;
  const float rate = g_turn_hold.rate;
  g_turn_hold.fresh = false;
  g_turn_hold.share = 0.0f;
  if (g_turn_hold.check_frames > 0 && --g_turn_hold.check_frames == 0 &&
      GodLog()) {
    if (g_turn_hold.check_version != g_god_version) {
      ModLog("god view     : turn check, the view moved again in the %d "
             "frames after the stop, so there is no check", kHoldCheckFrames);
    } else {
      ModLog("god view     : turn check, %d frames after the stop the "
             "heading is %.3f degrees, and it was %.3f degrees at the stop",
             kHoldCheckFrames, g_heading * 57.29578f,
             g_turn_hold.check_heading * 57.29578f);
    }
  }
  if (share == 0.0f) {
    StopTurnHold("the stick asks for no turn now", true);
    return;
  }
  const int direction = share > 0.0f ? 1 : -1;
  if (g_turn_hold.active && direction != g_turn_hold.direction) {
    StopTurnHold("the stick went the other way", true);
  }
  if (!g_turn_hold.active) {
    g_turn_hold.active = true;
    g_turn_hold.direction = direction;
    g_turn_hold.start_heading = g_heading;
    g_turn_hold.degrees = 0.0f;
    g_turn_hold.seconds = 0.0;
    g_turn_hold.cpu = 0.0;
    g_turn_hold.frames = 0;
    g_turn_hold.check_frames = 0;
    ++g_turn_holds;
    if (GodLog()) {
      ModLog("god view     : the smooth turn starts to the %s, %.0f%% of a "
             "full push, at %.0f degrees a second for a full push "
             "(smooth_turn_speed), heading %.2f degrees, frame %llu",
             direction > 0 ? "right" : "left", fabsf(share) * 100.0f, rate,
             g_heading * 57.29578f, ControllersFrameCount());
    }
  }
  const float degrees = comfort_math::SmoothTurnDegrees(
      share, rate, frame_seconds, kHoldFrameCap);
  const float seconds = zoom_math::FrameSeconds(frame_seconds, kHoldFrameCap);
  ++g_turn_hold.frames;
  g_turn_hold.seconds += seconds;
  g_turn_hold.cpu += zoom_math::FrameSeconds(cpu_seconds, kHoldFrameCap);
  if (degrees != 0.0f) {
    float room[3], head[3];
    TableRoom(room);
    HeadRoom(head);
    TablePose next;
    GodTurn(Pose(), room, head, degrees / 57.29578f, &next);
    g_center[0] = ClampMap(next.center[0]);
    g_center[2] = ClampMap(next.center[2]);
    g_center[1] = next.center[1];
    g_heading = Wrap(next.heading);
    g_scale = next.scale;
    Derive();
    ++g_serial;
    ++g_god_version;
    g_turn_hold.degrees += degrees;
    g_turn_hold.help += fabsf(degrees);
    g_turn_hold.help_direction = direction;
  }
  if (g_turn_holds <= kTurnFrameLogHolds &&
      g_turn_hold.frames <= kTurnFrameLogFrames) {
    ModLog("god view     : smooth turn frame %llu of the hold: %.3f degrees "
           "in %.4f s at %.0f%% of a full push, heading %.3f degrees, frame "
           "%llu", g_turn_hold.frames, degrees, seconds, share * 100.0f,
           g_heading * 57.29578f, ControllersFrameCount());
  }
}

double Seconds() {
  static LARGE_INTEGER frequency = {};
  if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
  LARGE_INTEGER count;
  QueryPerformanceCounter(&count);
  return frequency.QuadPart > 0
             ? static_cast<double>(count.QuadPart) / frequency.QuadPart
             : GetTickCount64() / 1000.0;
}

// THE END OF A HOLD of the smooth zoom: the stick came back, or the view
// cannot zoom. A request that waits is dropped. One log line gives the scale
// at the start and at the end, the time of the frames of the hold, which
// point the zoom kept and how far that point moved in the room, and how far
// the land point under the head and the render head moved across. The
// numbers are the ones of the last frame of the hold, so the view of now is
// not read. The check of ApplyGodZoomHold starts.
//
// view_stays: the view stays where the hold left it, so its version goes up
// one more time. The game camera then gets the place of the end of the hold,
// and the pair of TabletopGodCameraWritten gets the scale of the end. It is
// false when the view is built again.
void StopZoomHold(const char* cause, bool view_stays) {
  g_hold_solid_said = false;
  g_hold.fresh = false;
  g_hold.push = 0.0f;
  if (!g_hold.active) return;
  g_hold.active = false;
  ++g_holds;
  g_hold_seconds += g_hold.seconds;
  // A hold that changed nothing was at a limit of the scale.
  if (g_hold.moved == 0) ++g_god_refused;
  if (g_hold.moved != 0 && view_stays) {
    ++g_god_version;
    g_god_nonturn_version = g_god_version;
  }
  const bool at_limit = g_hold.direction > 0 ? g_hold.scale <= GodLowest()
                                             : g_hold.scale >= GodHighest();
  float eye[3] = {0.0f, 0.0f, 0.0f};
  const bool have_eye = HeadCameraTableEye(eye);
  g_hold_check.frames_left = kHoldCheckFrames;
  g_hold_check.scale = g_hold.scale;
  g_hold_check.version = g_god_version;
  g_hold_check.have_eye = have_eye;
  for (int i = 0; i < 3; ++i) g_hold_check.eye[i] = eye[i];
  if (!ZoomLog()) return;
  char drawn[96];
  if (g_hold.have_eye && have_eye && g_hold.scale > 0.0f) {
    const float dx = eye[0] - g_hold.start_eye[0];
    const float dz = eye[2] - g_hold.start_eye[2];
    sprintf_s(drawn, "the drawn head moved %.2f cm of the room",
              sqrtf(dx * dx + dz * dz) / g_hold.scale * 100.0f);
  } else {
    sprintf_s(drawn, "the drawn head is not known");
  }
  const float ux = g_hold.under[0] - g_hold.start_under[0];
  const float uz = g_hold.under[1] - g_hold.start_under[1];
  float point_off = 0.0f;
  for (int i = 0; i < 3; ++i) {
    const float d = g_hold.point_now[i] - g_hold.point_start[i];
    point_off += d * d;
  }
  char hill[128] = "";
  if (g_hold.sunk > 0.0005f) {
    sprintf_s(hill, " The land went %.1f cm down in the room, so the land "
              "under your head is %.0f cm below it.", g_hold.sunk * 100.0f,
              kGroundNearest * 100.0f);
  }
  ModLog("god view     : the smooth zoom stopped (%s). It zoomed %s from %.2f "
         "to %.2f units per meter, which is %.2f zoom steps. The stick was "
         "held for %.3f s, and the scale changed on %llu of %llu frames%s. "
         "The point of the zoom at %.1f %.1f %.1f moved %.2f cm in the "
         "room.%s The land under your head was at %.1f %.1f and is at %.1f "
         "%.1f, %.2f cm of the room apart, and %s.", cause,
         g_hold.direction > 0 ? "in" : "out", g_hold.start_scale,
         g_hold.scale,
         zoom_math::Steps(g_hold.start_scale, g_hold.scale,
                          1.0f + g_settings.god_zoom_pct * 0.01f),
         g_hold.seconds, g_hold.moved, g_hold.frames,
         at_limit ? ", and the zoom is at its limit" : "",
         g_hold.point.pivot[0], g_hold.point.pivot[1], g_hold.point.pivot[2],
         sqrtf(point_off) * 100.0f, hill,
         g_hold.start_under[0], g_hold.start_under[1], g_hold.under[0],
         g_hold.under[1],
         g_hold.scale > 0.0f
             ? sqrtf(ux * ux + uz * uz) / g_hold.scale * 100.0f
             : 0.0f,
         drawn);
  ModLog("god view     : in the smooth zoom the land under your head went "
         "from %.2f m to %.2f m below your head (god_zoom_far_level=%d)",
         g_hold.start_below, g_hold.below,
         g_settings.god_zoom_far_level != 0 ? 1 : 0);
}

// THE SMOOTH ZOOM (god_zoom_smooth=1, docs/god-view.md, "The zoom"), once a
// frame, at the main camera call. While the stick holds a zoom, the scale
// changes by the ratio of the time of this frame (zoom_math.h), about the
// point of a zoom step (GodZoomPoint), between the limits of the scale. With
// god_zoom_about 1 and 2 the hold takes the point one time, at its start, and
// keeps that point of the world. So the point does not move while the head
// turns a little. With god_zoom_about=0 each frame takes the land under the
// head again.
//
// The pump of the controllers asks again before each frame
// (TabletopGodZoomHold). A frame with no request changes nothing, so the zoom
// stops in the frame in which the stick comes back, and nothing glides. The
// scale changes here only, so each reader of a frame gets one scale.
//
// A frame that changes the scale sends the game's zoom event, as the game
// does on each update in which its own camera zooms (0x0045C38E). There is no
// blink.
//
// THE GAME CAMERA. With god_zoom_about=0 the point of the zoom is the place
// of the game camera (god_camera_at=1), so a hold does not move that place.
// The version of the view goes up on the first frame that changes the scale,
// as for a zoom step, and one more time at the end of the hold
// (StopZoomHold). So the game camera is written at most two times for a
// hold, and not on each frame. With a point that the hold keeps, the head
// moves over the land in each frame. The version then goes up on each frame
// that changes the scale, as for a grab, so the game camera follows the head.
//
// The log has one line for the start of a hold, one for its end, and one for
// the check kHoldCheckFrames frames later: the scale and the render head
// then against the ones at the stop.
void ApplyGodZoomHold(double frame_seconds) {
  const float push = g_hold.fresh ? g_hold.push : 0.0f;
  g_hold.fresh = false;
  g_hold.push = 0.0f;
  if (g_hold_check.frames_left > 0 && --g_hold_check.frames_left == 0 &&
      ZoomLog()) {
    float eye[3];
    if (g_hold_check.version != g_god_version) {
      ModLog("god view     : smooth zoom check, the view moved again in the "
             "%d frames after the stop, so there is no check",
             kHoldCheckFrames);
    } else if (g_hold_check.have_eye && HeadCameraTableEye(eye) &&
               g_scale > 0.0f) {
      const float dx = eye[0] - g_hold_check.eye[0];
      const float dz = eye[2] - g_hold_check.eye[2];
      ModLog("god view     : smooth zoom check, %d frames after the stop the "
             "scale is %.2f units per meter, and it was %.2f at the stop. "
             "The drawn head moved %.2f cm of the room in those frames.",
             kHoldCheckFrames, g_scale, g_hold_check.scale,
             sqrtf(dx * dx + dz * dz) / g_scale * 100.0f);
    } else {
      ModLog("god view     : smooth zoom check, %d frames after the stop the "
             "scale is %.2f units per meter, and it was %.2f at the stop",
             kHoldCheckFrames, g_scale, g_hold_check.scale);
    }
  }
  if (push == 0.0f) {
    StopZoomHold("the stick asks for no zoom now", true);
    return;
  }
  if (g_settings.god_zoom_smooth == 0) {
    StopZoomHold("the steps are chosen", true);
    return;
  }
  if ((CameraFeatures() & kFeatureZoom) == 0) {
    StopZoomHold("the game turned its zoom switch off", true);
    return;
  }
  const int direction = push > 0.0f ? 1 : -1;
  if (g_hold.active && direction != g_hold.direction) {
    StopZoomHold("the stick went the other way", true);
  }
  float room[3], head[3], under[3];
  TableRoom(room);
  HeadRoom(head);
  const TablePose pose = Pose();
  PoseWorld(pose, room, head, under);
  ZoomPoint point = g_hold.point;
  if (!g_hold.active || !g_hold.kept) {
    GodZoomPoint(pose, room, head, direction, &point);
  }
  const float step = 1.0f + g_settings.god_zoom_pct * 0.01f;
  if (!g_hold.active) {
    g_hold.active = true;
    g_hold.direction = direction;
    g_hold.kept = g_settings.god_zoom_about != zoom_math::kAboutHead;
    g_hold.point = point;
    PoseRoom(pose, room, point.pivot, g_hold.point_start);
    for (int i = 0; i < 3; ++i) g_hold.point_now[i] = g_hold.point_start[i];
    g_hold.sunk = 0.0f;
    g_hold.start_scale = g_hold.scale = pose.scale;
    g_hold.start_under[0] = g_hold.under[0] = under[0];
    g_hold.start_under[1] = g_hold.under[1] = under[2];
    g_hold.start_below = g_hold.below =
        head[1] - GroundInRoom(pose, room, LandUnderHead(pose, room, head));
    g_hold.have_eye = HeadCameraTableEye(g_hold.start_eye);
    g_hold.seconds = 0.0;
    g_hold.frames = g_hold.moved = 0;
    g_hold_check.frames_left = 0;
    if (ZoomLog()) {
      char why[224];
      ZoomPointText(point, why, sizeof(why));
      ModLog("god view     : the smooth zoom starts to zoom %s at %.2f units "
             "per meter, about the point at %.1f %.1f %.1f, %.2f m below "
             "your head: %s. %sA full push changes the scale %.2f times in "
             "%d ms.",
             direction > 0 ? "in" : "out", pose.scale, point.pivot[0],
             point.pivot[1], point.pivot[2], point.below, why,
             g_hold.kept ? "The hold keeps this point of the world. " : "",
             step, g_settings.god_zoom_smooth_ms);
    }
  }
  const float seconds = zoom_math::FrameSeconds(frame_seconds, kHoldFrameCap);
  const float wanted = zoom_math::FrameRatio(
      push, step, g_settings.god_zoom_smooth_ms * 0.001f, seconds);
  ++g_hold.frames;
  g_hold.seconds += seconds;
  TablePose next = pose;
  SolidStop stop;
  const float ratio =
      ZoomSolidRatio(pose, room, head, point.pivot, wanted, &stop);
  const float used = ratio == 1.0f
                         ? 1.0f
                         : GodZoomAbout(pose, point.pivot, ratio, GodLowest(),
                                        GodHighest(), &next);
  // One line for a hold that a solid stops.
  if (stop.short_step && !g_hold_solid_said) {
    g_hold_solid_said = true;
    SayZoomSolid(stop, used == 1.0f, pose.scale, next.scale,
                 "the smooth zoom");
  }
  // At a limit of the scale, or in a frame with no time, nothing moves.
  if (used == 1.0f) return;
  // The head moves over the land, and a hill there stays below it.
  if (g_hold.kept) g_hold.sunk += ZoomClearHead(&next, room, head);
  for (int i = 0; i < 3; i += 2) next.center[i] = ClampMap(next.center[i]);
  CameraZoneLimitMove(g_center, &next.center[0], &next.center[2],
                      "the smooth zoom");
  for (int i = 0; i < 3; i += 2) g_center[i] = next.center[i];
  g_center[1] = next.center[1];
  g_scale = next.scale;
  Derive();
  ++g_serial;
  if (g_hold.moved == 0 || g_hold.kept) {
    ++g_god_version;
    g_god_nonturn_version = g_god_version;
  }
  ++g_hold.moved;
  g_hold.scale = g_scale;
  PoseWorld(Pose(), room, head, under);
  g_hold.under[0] = under[0];
  g_hold.under[1] = under[2];
  g_hold.below = head[1] - GroundInRoom(Pose(), room,
                                        LandUnderHead(Pose(), room, head));
  PoseRoom(Pose(), room, g_hold.point.pivot, g_hold.point_now);
  SayLowest(Pose(), room, head);
  Help(kHelpZoom, kInputWheel);
}

bool WalkLog() {
  if (g_walk_logs >= kMaxWalkLogs) return false;
  ++g_walk_logs;
  return true;
}

const char* WalkRuleText(int rule) {
  return rule == walk_math::kRuleController
             ? "the direction in which the controller points"
             : "the direction of your head";
}

// The speed ramp, for the log.
const char* RampText() {
  static char text[192];
  if (g_settings.move_stick_ramp == 0) {
    return "The speed stays the speed of the push (move_stick_ramp=0)";
  }
  sprintf_s(text, "After %.0f s of a push of %.0f%% or more the speed doubles "
            "each %.0f s, up to %d times (move_stick_ramp=1)",
            walk_math::kRampHoldSeconds, walk_math::kRampShare * 100.0f,
            walk_math::kRampDoubleSeconds, g_settings.move_stick_ramp_most);
  return text;
}

// THE SPEED BY THE SCALE (move_stick_scale, walk_math::ScaleFactor): the
// factor of the speed of the stick at a scale, 1 when the key is 0.
float MoveScaleFactor(float scale) {
  if (g_settings.move_stick_scale == 0) return 1.0f;
  return walk_math::ScaleFactor(
      scale, g_settings.move_stick_scale_ref_x10 * 0.1f,
      g_settings.move_stick_scale_power_pct * 0.01f,
      g_settings.move_stick_scale_low_pct * 0.01f,
      g_settings.move_stick_scale_high_pct * 0.01f);
}

// The speed by the scale, for the log.
const char* MoveScaleText(float scale) {
  static char text[224];
  if (g_settings.move_stick_scale == 0) {
    return "The speed is the same at each scale (move_stick_scale=0)";
  }
  sprintf_s(text, "At %.2f units per meter the speed of the stick is times "
            "%.2f (move_stick_scale=1: (%.1f / scale) ^ %.2f, from %.2f to "
            "%.2f)", scale, MoveScaleFactor(scale),
            g_settings.move_stick_scale_ref_x10 * 0.1f,
            g_settings.move_stick_scale_power_pct * 0.01f,
            g_settings.move_stick_scale_low_pct * 0.01f,
            g_settings.move_stick_scale_high_pct * 0.01f);
  return text;
}

// The stick flies (move_type=1).
bool Flies() { return g_settings.move_type == walk_math::kTypeFly; }

// The word of the move for the log: "walk" or "fly".
const char* MoveWord() { return Flies() ? "fly" : "walk"; }

const char* FollowText() {
  if (Flies()) {
    return "The view does not follow the land. The land under your head "
           "stays 10 cm or more below your head, and your head at most 10000 "
           "units over the land (move_type=1)";
  }
  return g_settings.move_stick_follow_land != 0
             ? "The height of the view follows the land under your head "
               "(move_stick_follow_land=1)"
             : "The view keeps its height (move_stick_follow_land=0)";
}

// THE END OF A WALK with the stick: the stick came back, or the player
// cannot move now. A request that waits is dropped. One log line gives the
// place at the start and at the end, how far the player went over the land
// and in the room, in which direction of the room, and for how long. The
// check of ApplyGodWalk starts.
//
// view_stays: the view stays where the walk left it. It is false when the
// view is built again, and then there is no check.
void StopWalk(const char* cause, bool view_stays) {
  g_walk.fresh = false;
  g_walk.have_aim = false;
  g_walk.have_aim3 = false;
  g_walk.push[0] = g_walk.push[1] = 0.0f;
  if (!g_walk.active) return;
  g_walk.active = false;
  ++g_walks;
  g_walk_seconds += g_walk.seconds;
  if (hint_learn_math::WalkMoved(sqrtf(g_walk.went[0] * g_walk.went[0] +
                                       g_walk.went[1] * g_walk.went[1] +
                                       g_walk.climb * g_walk.climb))) {
    HintCardLearnDone(hint_learn_math::kSkillWalk,
                      "the stick moved the player");
  }
  g_walk_check.frames_left = 0;
  if (view_stays) {
    g_walk_check.frames_left = kWalkCheckFrames;
    g_walk_check.version = g_god_version;
    g_walk_check.center[0] = g_center[0];
    g_walk_check.center[1] = g_center[2];
    g_walk_check.have_eye = HeadCameraTableEye(g_walk_check.eye);
  }
  if (!WalkLog()) return;
  const float dx = g_center[0] - g_walk.start_center[0];
  const float dz = g_center[2] - g_walk.start_center[1];
  const float room_m =
      sqrtf(g_walk.went[0] * g_walk.went[0] + g_walk.went[1] * g_walk.went[1]);
  const float degrees = walk_math::DegreesRight(g_walk.went);
  char hill[128] = "";
  if (g_walk.sunk > 0.0005f) {
    sprintf_s(hill, " The land went %.1f cm down in the room, so a hill under "
              "your head is %.0f cm below it.", g_walk.sunk * 100.0f,
              kGroundNearest * 100.0f);
  }
  char blind[128] = "";
  if (g_walk.blind != 0) {
    sprintf_s(blind, " On %llu frames the controller pointed too far up or "
              "down for a direction.", g_walk.blind);
  }
  char land[256] = "";
  if (g_walk.have_below && g_walk.type == walk_math::kTypeFly) {
    sprintf_s(land, " The land under your head was %.1f cm below your head "
              "and is %.1f cm below it. The fly went %.1f cm %s in the room "
              "(move_type=1).",
              g_walk.start_below * 100.0f, g_walk.below * 100.0f,
              fabsf(g_walk.climb) * 100.0f,
              g_walk.climb < 0.0f ? "down" : "up");
  } else if (g_walk.have_below) {
    sprintf_s(land, " The land under your head was %.1f cm below your head "
              "and is %.1f cm below it. The view went %.1f cm up and %.1f cm "
              "down with the land (move_stick_follow_land=%d).",
              g_walk.start_below * 100.0f, g_walk.below * 100.0f,
              g_walk.rose * 100.0f, g_walk.fell * 100.0f,
              g_settings.move_stick_follow_land);
  }
  char late[224] = "";
  if (g_walk.shown != 0) {
    sprintf_s(late, " Of %llu frames of the headset, %llu came more than one "
              "refresh of %.1f ms after the frame before, and the headset "
              "showed the frame before again for them.", g_walk.shown,
              g_walk.late, g_walk.period / 1e6);
  }
  char floor[288] = "";
  if (g_walk.type == walk_math::kTypeFly) {
    sprintf_s(floor, " The floor of the fly lifted the view %.1f cm on %llu "
              "frames, and the least clearance of your head after a step was "
              "%.1f cm. The fly met %llu solids: %llu steps slid past one, "
              "and %llu steps stopped at one.",
              g_walk.lifted * 100.0f, g_walk.lift_frames,
              g_walk.least_clear * 100.0f, g_walk.solids_met,
              g_walk.solid_slides, g_walk.solid_stops);
  }
  char ramped[96] = "";
  if (g_walk.ramp_top > 1.0f) {
    sprintf_s(ramped, " The speed ramp went up to %.2f times the speed of "
              "the push (move_stick_ramp=1).", g_walk.ramp_top);
  }
  char scaled[128] = "";
  if (g_settings.move_stick_scale != 0) {
    sprintf_s(scaled, " The scale made the speed of the stick %.2f to %.2f "
              "times move_stick_speed_cm (move_stick_scale=1).",
              g_walk.scale_least, g_walk.scale_most);
  }
  char even[160] = "";
  if (g_walk.moved != 0) {
    sprintf_s(even, " A frame took %.1f to %.1f ms, and a step was %.2f to "
              "%.2f cm of the room.", g_walk.frame_least * 1000.0f,
              g_walk.frame_most * 1000.0f, g_walk.step_least * 100.0f,
              g_walk.step_most * 100.0f);
  }
  ModLog("god view     : the %s with the stick stopped (%s). You went %.2f "
         "units over the land, which is %.2f cm of the room, %.1f degrees to "
         "the %s of straight ahead in the room. The land under your head was "
         "at %.1f %.1f and is at %.1f %.1f. The stick was pushed for %.3f s, "
         "which gives %.1f cm of the room each second, and the view moved on "
         "%llu of %llu frames (move_stick=%d, %.2f to %.2f units per "
         "meter), frame %llu.%s%s%s%s%s%s%s%s",
         g_walk.type == walk_math::kTypeFly ? "fly" : "walk", cause,
         sqrtf(dx * dx + dz * dz), room_m * 100.0f, fabsf(degrees),
         degrees < -0.05f ? "left" : "right", g_walk.start_under[0],
         g_walk.start_under[1], g_walk.under[0], g_walk.under[1],
         g_walk.seconds,
         g_walk.seconds > 0.0 ? g_walk.path * 100.0f / g_walk.seconds : 0.0,
         g_walk.moved, g_walk.frames, g_walk.rule, g_walk.start_scale, g_scale,
         ControllersFrameCount(), hill, blind, land, floor, ramped, scaled,
         even, late);
}

// THE WALK WITH THE STICK (move_stick, docs/god-view.md, "The walk with the
// stick"), once a frame, at the main camera call. While the stick of the hand
// that does not point is pushed, the player moves over the land: the waist
// point goes to the land that was one step from it in the room
// (walk_math.h). It is the pan of a grab of the world, so the height of the
// ground, the heading and the scale stay.
//
// FORWARD is the forward direction of the head (move_stick=1) or the
// direction in which the controller of the stick points (2), flat on the
// land, taken again in each frame. A controller that points almost straight
// up or down has no direction on the land (walk_math::kAimLeast): the walk
// then keeps the direction of the frame before, and a walk with no such
// frame does not move.
//
// THE SPEED is a speed of the room: move_stick_speed_cm at a full push,
// times the factor of the scale of this frame (move_stick_scale,
// MoveScaleFactor). So the walk is faster at a small scale and a little
// slower at a large one. The factor depends on the scale only, and in a
// smooth zoom during a walk it follows the scale of each frame. The step of
// a frame is the speed times the real time of the frame, with the cap of
// the smooth zoom.
//
// THE FLY (move_type=1). Forward is the forward direction of the head or of
// the controller in 3D, so a push while the player looks or points down
// goes down. The stick to the side moves level, at right angles to forward
// (walk_math::FlyRight). The view does not follow the land. The height has
// the floor of the grab (the land under the head stays 10 cm or more below
// the head) and a ceiling of kFlyCeilingUnits world units over the land
// (walk_math::FlyCeiling). At the ceiling only the move up stops. The floor
// has the measure of the head blinder at the place of the head after the
// step (FlyLandClearance, walk_math::FlyFloor), so a fly across a rising
// slope slides up along it and the overlay does not start. With
// god_zoom_solid_stop=1 the head stays god_zoom_solid_cm out of a house, a
// rock or a tree trunk: the step slides past it, and only a push straight
// at it stops (FlySolidStop, fly_solid_slide).
//
// THE SPEED RAMP (move_stick_ramp=1, walk_math.h). While the stick is pushed
// to 90 percent or more, the speed of the walk and of the fly grows with
// the time of that push, up to move_stick_ramp_most times. A smaller push
// goes back to the speed of the push in the same frame.
//
// The pump of the controllers asks again before each frame
// (TabletopGodWalk). A frame with no request moves nothing, so the walk
// stops in the frame in which the stick comes back, and nothing glides.
//
// THE RULES OF A GRAB hold (TabletopMove). The game's switch for the drag of
// the land (bit 8) must be on. The waist point stays on the map. Each frame
// that moves sends the drag event of the game (0x308), and the version of
// the view goes up, so the game camera follows the player. A hill at the new
// place of the head stays below the head (ZoomClearHead).
//
// The log has one line for the start of a walk, one for its end, and one for
// the check kWalkCheckFrames frames later.
void ApplyGodWalk(double frame_seconds) {
  const bool fresh = g_walk.fresh;
  const float push[2] = {fresh ? g_walk.push[0] : 0.0f,
                         fresh ? g_walk.push[1] : 0.0f};
  const bool have_aim = fresh && g_walk.have_aim;
  const float aim[2] = {g_walk.aim[0], g_walk.aim[1]};
  const bool have_aim3 = fresh && g_walk.have_aim3;
  const float aim3[3] = {g_walk.aim3[0], g_walk.aim3[1], g_walk.aim3[2]};
  g_walk.fresh = false;
  g_walk.have_aim = false;
  g_walk.have_aim3 = false;
  g_walk.push[0] = g_walk.push[1] = 0.0f;
  if (g_walk_check.frames_left > 0 && --g_walk_check.frames_left == 0 &&
      WalkLog()) {
    float eye[3];
    if (g_walk_check.version != g_god_version) {
      ModLog("god view     : walk check, the view moved again in the %d "
             "frames after the stop, so there is no check", kWalkCheckFrames);
    } else {
      const float dx = g_center[0] - g_walk_check.center[0];
      const float dz = g_center[2] - g_walk_check.center[1];
      char drawn[96] = "the drawn head is not known";
      if (g_walk_check.have_eye && HeadCameraTableEye(eye) && g_scale > 0.0f) {
        const float ex = eye[0] - g_walk_check.eye[0];
        const float ez = eye[2] - g_walk_check.eye[2];
        sprintf_s(drawn, "the drawn head moved %.2f cm of the room",
                  sqrtf(ex * ex + ez * ez) / g_scale * 100.0f);
      }
      ModLog("god view     : walk check, %d frames after the stop the waist "
             "point is at %.2f %.2f, and it was at %.2f %.2f at the stop: "
             "%.3f units apart. In those frames %s.", kWalkCheckFrames,
             g_center[0], g_center[2], g_walk_check.center[0],
             g_walk_check.center[1], sqrtf(dx * dx + dz * dz), drawn);
    }
  }
  if (push[0] == 0.0f && push[1] == 0.0f) {
    StopWalk("the stick is back", true);
    return;
  }
  const int rule = walk_math::Rule(g_settings.move_stick);
  if (rule == walk_math::kRuleOff) {
    StopWalk("move_stick is 0", true);
    return;
  }
  if ((CameraFeatures() & kFeatureDrag) == 0) {
    StopWalk("the game turned its switch for the drag of the land off", true);
    return;
  }
  if (g_walk.active && rule != g_walk.rule) {
    StopWalk("the rule of the direction changed", true);
  }
  const int type = walk_math::Type(g_settings.move_type);
  const bool fly = type == walk_math::kTypeFly;
  if (g_walk.active && type != g_walk.type) {
    StopWalk("move_type changed", true);
  }
  float room[3], head[3], under[3];
  TableRoom(room);
  HeadRoom(head);
  const TablePose pose = Pose();
  PoseWorld(pose, room, head, under);
  // Forward on the land, in the room.
  float forward[2] = {0.0f, 1.0f};
  bool have_forward = false;
  // THE FLY: forward in 3D, and its level right. A forward that points
  // almost straight up or down strafes along the level right of the head.
  float forward3[3] = {0.0f, 0.0f, 1.0f};
  float right[2] = {g_walk.right[0], g_walk.right[1]};
  if (fly) {
    float matrix[9], lean[3], flat[2];
    const bool have_head = HeadPoseCurrent(matrix, lean);
    float head_right[2] = {g_walk.right[0], g_walk.right[1]};
    if (have_head && walk_math::HeadForward(matrix, flat)) {
      head_right[0] = flat[1];
      head_right[1] = -flat[0];
    }
    if (rule == walk_math::kRuleHead) {
      have_forward = have_head && zoom_math::HeadForward(matrix, forward3);
    } else if (have_aim3) {
      have_forward = walk_math::Unit(aim3, forward3);
    }
    if (have_forward) {
      walk_math::FlyRight(forward3, head_right, walk_math::kAimLeast, right);
      forward[0] = forward3[0];
      forward[1] = forward3[2];
    }
  } else if (rule == walk_math::kRuleHead) {
    float matrix[9], lean[3];
    have_forward = HeadPoseCurrent(matrix, lean) &&
                   walk_math::HeadForward(matrix, forward);
  } else if (have_aim) {
    forward[0] = aim[0];
    forward[1] = aim[1];
    have_forward = true;
  }
  if (!g_walk.active) {
    g_walk.active = true;
    g_walk.rule = rule;
    g_walk.type = type;
    g_walk.climb = 0.0f;
    g_walk.ramp_held = 0.0f;
    g_walk.ramp_top = 1.0f;
    g_walk.scale_least = g_walk.scale_most = MoveScaleFactor(pose.scale);
    g_walk.lifted = 0.0f;
    g_walk.lift_frames = 0;
    g_walk.least_clear = 1000.0f;
    g_walk.solid_stops = 0;
    g_walk.solid_slides = 0;
    g_walk.solid_last = nullptr;
    g_walk.solid_way = 0;
    g_walk.solids_met = 0;
    g_walk.have_forward = false;
    g_walk.start_center[0] = pose.center[0];
    g_walk.start_center[1] = pose.center[2];
    g_walk.start_under[0] = g_walk.under[0] = under[0];
    g_walk.start_under[1] = g_walk.under[1] = under[2];
    g_walk.start_scale = pose.scale;
    g_walk.went[0] = g_walk.went[1] = 0.0f;
    g_walk.path = 0.0f;
    g_walk.sunk = 0.0f;
    g_walk.seconds = 0.0;
    g_walk.frames = g_walk.moved = g_walk.blind = 0;
    g_walk.frame_least = g_walk.frame_most = 0.0f;
    g_walk.step_least = g_walk.step_most = 0.0f;
    g_walk.rose = g_walk.fell = 0.0f;
    g_walk.shown_at = g_walk.period = 0;
    g_walk.shown = g_walk.late = 0;
    g_walk.have_below = pose.scale > 0.0f;
    g_walk.start_below = g_walk.below =
        g_walk.have_below
            ? head[1] - GroundInRoom(pose, room, LandUnderHead(pose, room, head))
            : 0.0f;
    g_walk_check.frames_left = 0;
    if (WalkLog()) {
      char where[128] = "has no direction on the land now";
      if (have_forward && fly) {
        const float degrees = walk_math::DegreesRight(forward);
        const float down = walk_math::DegreesDown(forward3);
        sprintf_s(where, "is %.1f degrees to the %s of straight ahead in the "
                  "room and %.1f degrees %s level", fabsf(degrees),
                  degrees < -0.05f ? "left" : "right", fabsf(down),
                  down < 0.0f ? "above" : "below");
      } else if (fly) {
        sprintf_s(where, "has no direction now");
      } else if (have_forward) {
        const float degrees = walk_math::DegreesRight(forward);
        sprintf_s(where, "is %.1f degrees to the %s of straight ahead in the "
                  "room", fabsf(degrees), degrees < -0.05f ? "left" : "right");
      }
      ModLog("god view     : the %s with the stick starts at %.2f units per "
             "meter, with the land under your head at %.1f %.1f, %.1f cm "
             "below your head. Forward is %s (move_stick=%d), and it %s. A "
             "full push moves you %d cm of the room each second "
             "(move_stick_speed_cm). %s: %.0f cm of the room each second. "
             "%s. %s, frame %llu.",
             MoveWord(), pose.scale, under[0], under[2],
             g_walk.start_below * 100.0f, WalkRuleText(rule), rule, where,
             g_settings.move_stick_speed_cm, MoveScaleText(pose.scale),
             g_settings.move_stick_speed_cm * MoveScaleFactor(pose.scale),
             FollowText(), RampText(), ControllersFrameCount());
    }
  }
  const float seconds = zoom_math::FrameSeconds(frame_seconds, kHoldFrameCap);
  ++g_walk.frames;
  g_walk.seconds += seconds;
  if (seconds > 0.0f) {
    if (g_walk.frame_most == 0.0f || seconds < g_walk.frame_least) {
      g_walk.frame_least = seconds;
    }
    if (seconds > g_walk.frame_most) g_walk.frame_most = seconds;
  }
  if (have_forward) {
    g_walk.forward[0] = forward[0];
    g_walk.forward[1] = forward[1];
    for (int i = 0; i < 3; ++i) g_walk.forward3[i] = forward3[i];
    g_walk.right[0] = right[0];
    g_walk.right[1] = right[1];
    g_walk.have_forward = true;
  } else {
    // The direction of the frame before, when the walk has one.
    ++g_walk.blind;
    if (!g_walk.have_forward) return;
    forward[0] = g_walk.forward[0];
    forward[1] = g_walk.forward[1];
    for (int i = 0; i < 3; ++i) forward3[i] = g_walk.forward3[i];
    right[0] = g_walk.right[0];
    right[1] = g_walk.right[1];
  }
  // THE SPEED RAMP: the time of the full push, and the factor of the speed.
  const float share = sqrtf(push[0] * push[0] + push[1] * push[1]);
  g_walk.ramp_held =
      g_settings.move_stick_ramp != 0
          ? walk_math::RampSeconds(g_walk.ramp_held, share, seconds)
          : 0.0f;
  const float ramp = walk_math::RampFactor(
      g_walk.ramp_held, static_cast<float>(g_settings.move_stick_ramp_most));
  if (ramp > g_walk.ramp_top) g_walk.ramp_top = ramp;
  // THE SPEED BY THE SCALE of this frame. A smooth zoom of the other stick
  // changes the scale between two frames of the walk, and the factor follows.
  const float scale_factor = MoveScaleFactor(pose.scale);
  if (scale_factor < g_walk.scale_least) g_walk.scale_least = scale_factor;
  if (scale_factor > g_walk.scale_most) g_walk.scale_most = scale_factor;
  const float top =
      g_settings.move_stick_speed_cm * 0.01f * scale_factor * ramp;
  TablePose next;
  if (fly) {
    float speed[3], step[3];
    walk_math::FlySpeed(forward3, right, push, top, speed);
    walk_math::FlyStep(speed, seconds, step);
    // In a frame with no time, nothing moves.
    if (step[0] == 0.0f && step[1] == 0.0f && step[2] == 0.0f) return;
    walk_math::Fly(pose, step, &next);
  } else {
    float speed[2], step[2];
    walk_math::Speed(forward, push, top, speed);
    walk_math::Step(speed, seconds, step);
    // In a frame with no time, nothing moves.
    if (step[0] == 0.0f && step[1] == 0.0f) return;
    walk_math::Walk(pose, step, &next);
  }
  // The waist point stays on the map, as for a grab.
  next.center[0] = ClampCenter(next.center[0]);
  next.center[2] = ClampCenter(next.center[2]);
  // And at most god_zone_margin past the camera zone of the game.
  CameraZoneLimitMove(g_center, &next.center[0], &next.center[2],
                      fly ? "the fly with the stick" : "the walk with the stick");
  // THE HEIGHT OF A FLY: the land under the head stays kGroundNearest or
  // more below the head (FloorHeadY), and the head at most kFlyCeilingUnits
  // world units over the land. At a limit the height stops, and the move
  // across goes on.
  if (fly && next.scale > 0.0f) {
    next.center[1] = walk_math::FlyCeiling(
        next.center[1], g_center[1], LandUnderHead(next, room, head), room[1],
        next.scale, head[1], walk_math::kFlyCeilingUnits);
    FlySolidStop(pose, room, head, &next);
    const float before = FlyLandClearance(pose, room, head);
    const float after = FlyLandClearance(next, room, head);
    const float y = walk_math::FlyFloor(next.center[1], after, before,
                                        walk_math::kFlyClearMeters,
                                        next.scale);
    if (y > next.center[1]) {
      g_walk.lifted += (y - next.center[1]) / next.scale;
      ++g_walk.lift_frames;
    }
    next.center[1] = y;
  }
  if (next.center[0] == g_center[0] && next.center[2] == g_center[2] &&
      next.center[1] == g_center[1]) {
    return;
  }
  float took[2];
  walk_math::InRoom(pose, next.center[0] - pose.center[0],
                    next.center[2] - pose.center[2], took);
  const float rose_m = fly && next.scale > 0.0f
                           ? (next.center[1] - pose.center[1]) / next.scale
                           : 0.0f;
  const float way =
      sqrtf(took[0] * took[0] + took[1] * took[1] + rose_m * rose_m);
  if (g_walk.step_most == 0.0f || way < g_walk.step_least) {
    g_walk.step_least = way;
  }
  if (way > g_walk.step_most) g_walk.step_most = way;
  // THE LAND THAT THE VIEW FOLLOWS (move_stick_follow_land=1). The land
  // height under the head at the new place goes through the filter of
  // walk_math.h, and the view goes up or down by the change of its output,
  // so the filtered land keeps its height in the room. The filter starts
  // again when something else moved the view since the last frame of a walk
  // (walk_math::FollowRestart). The fly does not follow the land.
  if (!fly && g_settings.move_stick_follow_land != 0 && next.scale > 0.0f) {
    const uint64_t origin = HeadPoseOriginSerial();
    const bool changed = g_follow.version != g_god_version;
    const bool moved = g_follow.nonturn != g_god_nonturn_version;
    const bool recentered = g_follow.origin != origin;
    const bool placed = g_follow.placed;
    const int follow_rule = g_settings.move_stick_follow_place != 0
                                ? walk_math::kFollowPlaceEach
                                : walk_math::kFollowPlaceVersion;
    const bool again = walk_math::FollowRestart(
        follow_rule, g_follow.have, changed, moved, placed, recentered);
    // A log line when a place or a recenter is the only cause, so the log
    // shows the case of both rules.
    if (g_follow.have && (placed || recentered) && !moved && !changed &&
        g_follow_logs < 20 && WalkLog()) {
      ++g_follow_logs;
      const float old_land = g_follow.land.stage[1];
      const float land_now = LandUnderHead(pose, room, head);
      ModLog("god view     : %s since the last walk. The filter of the land "
             "holds %.2f, and the land under your head is at %.2f: %.1f cm "
             "of the room apart. %s (move_stick_follow_place=%d)",
             placed ? "the view went to a camera of the game"
                    : "the room was recentered",
             old_land, land_now,
             (land_now - old_land) / pose.scale * 100.0f,
             again ? "The filter starts again at the land under your head"
                   : "The filter goes on from the land of the last walk",
             g_settings.move_stick_follow_place != 0 ? 1 : 0);
    }
    if (again) {
      walk_math::FollowStart(LandUnderHead(pose, room, head), &g_follow.land);
      g_follow.have = true;
    }
    g_follow.placed = false;
    g_follow.origin = origin;
    const float rise =
        walk_math::FollowStep(&g_follow.land, LandUnderHead(next, room, head),
                              way, walk_math::kFollowLength);
    next.center[1] += rise;
    if (rise > 0.0f) {
      g_walk.rose += rise / next.scale;
    } else {
      g_walk.fell -= rise / next.scale;
    }
  }
  // The head moves over the land, and a hill there stays below it.
  // The fly has its own floor above.
  if (!fly) g_walk.sunk += ZoomClearHead(&next, room, head);
  for (int i = 0; i < 3; ++i) g_center[i] = next.center[i];
  Derive();
  ++g_serial;
  ++g_god_version;
  // The filter of the land counts only the walk: a walk after a fly starts
  // it again.
  if (!fly) g_follow.version = g_god_version;
  g_walk.climb += rose_m;
  // A walk is no snap turn: the game camera goes over the land under the
  // head, and does not turn about its focus (TabletopGodTurnedOnly).
  g_god_nonturn_version = g_god_version;
  if (!fly) g_follow.nonturn = g_god_nonturn_version;
  ++g_walk.moved;
  ++g_walk_moves;
  // THE FRAMES OF THE HEADSET. The step of a frame is the speed times the
  // time of that frame, so the walk is even in the time of the game. The
  // headset shows a frame at a refresh. When a frame comes more than one
  // refresh after the frame before, the headset shows the frame before
  // again. A runtime with no Space Warp corrects only the turn of the head
  // for it, so the land stops for that refresh and jumps at the next one.
  // The log of the end counts those frames.
  int64_t shown_at = 0, period = 0;
  if (StereoDisplayTime(&shown_at, &period) && shown_at != g_walk.shown_at) {
    if (g_walk.shown_at != 0 && period > 0) {
      ++g_walk.shown;
      if (shown_at - g_walk.shown_at > period + period / 2) ++g_walk.late;
      g_walk.period = period;
    }
    g_walk.shown_at = shown_at;
  }
  g_walk.went[0] += took[0];
  g_walk.went[1] += took[1];
  g_walk.path += sqrtf(took[0] * took[0] + took[1] * took[1]);
  const TablePose now = Pose();
  PoseWorld(now, room, head, under);
  g_walk.under[0] = under[0];
  g_walk.under[1] = under[2];
  const float land = LandUnderHead(now, room, head);
  g_walk.below = head[1] - GroundInRoom(now, room, land);
  // The test move_stick_trace: one line for each of the first frames.
  // The fly: the clearance of the head by the measure of the head blinder.
  const float clear_now = fly ? FlyLandClearance(now, room, head) : 1000.0f;
  if (clear_now < g_walk.least_clear) g_walk.least_clear = clear_now;
  const bool trace =
      g_walk.moved <= static_cast<uint64_t>(g_settings.move_stick_trace) &&
      g_walk_traces < kMaxWalkTraces;
  if (trace) ++g_walk_traces;
  if (trace && fly) {
    ModLog("god view     : fly trace, frame %llu of the fly: %.2f ms, a step "
           "of %.3f cm of the room (%.3f cm up, the speed times %.2f), the "
           "waist point at %.3f %.3f %.3f, the land under your head at %.3f "
           "and %.2f cm below your head, a clearance of %.2f cm, frame %llu",
           g_walk.frames, seconds * 1000.0f, way * 100.0f, rose_m * 100.0f,
           ramp, now.center[0], now.center[1], now.center[2], land,
           g_walk.below * 100.0f, clear_now * 100.0f, ControllersFrameCount());
  } else if (trace) {
    ModLog("god view     : walk trace, frame %llu of the walk: %.2f ms, a step "
           "of %.3f cm of the room (the speed times %.2f), the waist point at "
           "%.3f %.3f %.3f, the land under your head at %.3f (filtered %.3f) "
           "and %.2f cm below your head, frame %llu",
           g_walk.frames, seconds * 1000.0f, way * 100.0f, ramp, now.center[0],
           now.center[1], now.center[2], land,
           g_follow.have ? g_follow.land.stage[1] : land,
           g_walk.below * 100.0f, ControllersFrameCount());
  }
  Help(kHelpDrag, kInputButton);
}

// THE TELEPORT ON THE TABLE (play mode 3). The land point goes to the table
// center, at the same scale and heading.
void ApplyTableTeleport() {
  if (!g_tele.pending) return;
  g_tele.pending = false;
  const float from_x = g_center[0], from_z = g_center[2];
  g_center[0] = ClampCenter(g_tele.target[0]);
  g_center[1] = 0.0f;
  g_center[2] = ClampCenter(g_tele.target[2]);
  Derive();
  ++g_serial;
  ++g_tele_moves;
  ComfortCameraBlink();
  ModLog("teleport     : the table moved. The table center went from %.1f %.1f "
         "to %.1f %.1f.", from_x, from_z, g_center[0], g_center[2]);
  Help(TeleportJumpEvent(), kInputButton);
}

// THE EDGE OF THE TABLE. A block past the radius takes the reject path of
// the cull itself (0x00877CDD): no detail, flags 1 and 4 cleared, state 4. It
// then leaves the land, the reflection and the object list.
void __fastcall h_BlockCull(uint8_t* block) {
  float x0, z0;
  if (g_live && g_have_fit && g_have_table && BlockCorner(block, &x0, &z0)) {
    const int bx = static_cast<int>(x0 / kBlockSize);
    const int bz = static_cast<int>(z0 / kBlockSize);
    const bool land = bx >= 0 && bx < kMapBlocks && bz >= 0 &&
                      bz < kMapBlocks && g_block_land[bx][bz];
    if (BlockCut(x0, z0, kBlockSize, g_center, g_radius, land)) {
      *reinterpret_cast<int32_t*>(block + kBlockDetail) = 0;
      *reinterpret_cast<uint32_t*>(block + kBlockFlags) &= ~5u;
      *reinterpret_cast<int32_t*>(block + kBlockState) = 4;
      ++g_blocks_cut;
      return;
    }
    ++g_blocks_kept;
  }
  o_BlockCull(block);
}

// THE SEA STOPS AT THE EDGE. The plain sea calls this in place of the
// submit. With the table on, the quad becomes a disc of the table's radius,
// with the game's own colors, texture scroll, material and states.
void __fastcall SeaSubmit(int count, const float* positions,
                          const uint32_t* colors, const float* uvs,
                          int triangles, const void* indices, void* material,
                          int flags) {
  const SubmitFn submit = reinterpret_cast<SubmitFn>(kSubmit);
  if (!g_live || !g_have_table || count != 4 || colors == nullptr ||
      uvs == nullptr) {
    submit(count, positions, colors, uvs, triangles, indices, material, flags);
    return;
  }
  // The quad's first corner is (-70000, 0, 70000), and its texture point is
  // the scroll of this frame.
  const float s0 = uvs[0], t0 = uvs[1];
  for (int i = 0; i < kFanVertices; ++i) {
    SeaUv(g_fan_positions[i][0], g_fan_positions[i][2], s0, t0,
          &g_fan_uvs[i][0], &g_fan_uvs[i][1]);
    g_fan_colors[i] = colors[0];
  }
  submit(kFanVertices, &g_fan_positions[0][0], g_fan_colors, &g_fan_uvs[0][0],
         kFanSegments, &g_fan_indices[0][0], material, flags);
  ++g_sea_discs;
}

// Send the call of the plain sea to SeaSubmit. Only when the five bytes are
// the ones this was read from.
bool PatchSeaCall() {
  uint8_t* site = reinterpret_cast<uint8_t*>(kSeaCall);
  if (IsBadReadPtr(site, sizeof(kSeaCallBytes)) ||
      memcmp(site, kSeaCallBytes, sizeof(kSeaCallBytes)) != 0) {
    return false;
  }
  DWORD old = 0;
  if (!VirtualProtect(site, sizeof(kSeaCallBytes), PAGE_EXECUTE_READWRITE,
                      &old)) {
    return false;
  }
  const int32_t rel = static_cast<int32_t>(
      reinterpret_cast<uintptr_t>(&SeaSubmit) -
      (kSeaCall + sizeof(kSeaCallBytes)));
  memcpy(site + 1, &rel, sizeof(rel));
  VirtualProtect(site, sizeof(kSeaCallBytes), old, &old);
  FlushInstructionCache(GetCurrentProcess(), site, sizeof(kSeaCallBytes));
  return true;
}

// THE KEY COLOR PAST THE TABLE.
//
// The game does not clear the frame in play: the sky dome is the background.
// A clear of the key color cannot take the place of the sky: the sea is not
// opaque, and it would show the key color behind it. So the sky still draws,
// as the background of the sea. Then a clear of the key color covers the screen outside the outline
// of the sea disc, row by row, with the game's own device clear
// (fn_0082EE70). The land and the objects draw after it, so a mountain that
// stands up past the outline is not cut. The glare of the sun would draw
// over the key, so its request goes.
struct KeyRect {  // D3DRECT
  LONG x1, y1, x2, y2;
};
const int kMaxRows = 4096;
const int kMaxKeyRects = 2 * kMaxRows + 2;
KeyRect g_key_rects[kMaxKeyRects];
float g_row_left[kMaxRows];
float g_row_right[kMaxRows];
const uintptr_t kSunGlareWanted = 0x00FA275C;

LONG Clamp(float value, LONG low, LONG high) {
  if (!(value > static_cast<float>(low))) return low;
  if (value > static_cast<float>(high)) return high;
  return static_cast<LONG>(value);
}

// The rects outside the sea disc on the screen of the eye pass that runs.
// -1 when the screen is not known.
int KeyRects() {
  const bw::tech::InfoTransform* info =
      reinterpret_cast<const bw::tech::InfoTransform*>(
          bw::tech::kInfoTransform);
  const LONG w = info->res_x, h = info->res_y;
  if (w <= 0 || h <= 0 || h > kMaxRows) return -1;
  const float* m = reinterpret_cast<const float*>(bw::tech::kWorldToCamera);
  const float near_z = *reinterpret_cast<const float*>(bw::tech::kNearPlane);
  float camera[kFanSegments][3];
  for (int i = 0; i < kFanSegments; ++i) {
    ToCamera(m, g_fan_positions[i + 1], camera[i]);
  }
  float clipped[kFanSegments + 1][3];
  const int n = ClipNear(camera, kFanSegments, near_z, clipped);
  int count = 0;
  if (n < 3) {
    g_key_rects[count++] = {0, 0, w, h};
    return count;
  }
  float points[kFanSegments + 1][2];
  float top = 1e30f, bottom = -1e30f;
  for (int i = 0; i < n; ++i) {
    ToScreen(clipped[i], info->half_res_x, info->half_res_y, points[i]);
    if (points[i][1] < top) top = points[i][1];
    if (points[i][1] > bottom) bottom = points[i][1];
  }
  const LONG first = Clamp(floorf(top), 0, h);
  const LONG last = Clamp(ceilf(bottom), 0, h);
  if (first > 0) g_key_rects[count++] = {0, 0, w, first};
  const int rows = static_cast<int>(last - first);
  RowSpans(points, n, first, rows, g_row_left, g_row_right);
  for (int r = 0; r < rows; ++r) {
    const LONG y = first + r;
    if (g_row_left[r] > g_row_right[r]) {
      g_key_rects[count++] = {0, y, w, y + 1};
      continue;
    }
    // The pixels whose middle is outside the outline take the key. The sea
    // draws exactly the others.
    const LONG left = Clamp(KeyBefore(g_row_left[r]), 0, w);
    const LONG right = Clamp(KeyFrom(g_row_right[r]), 0, w);
    if (left > 0) g_key_rects[count++] = {0, y, left, y + 1};
    if (right < w) g_key_rects[count++] = {right, y, w, y + 1};
  }
  if (last < h) g_key_rects[count++] = {0, last, w, h};
  return count;
}

void __cdecl h_SkyDraw() {
  o_SkyDraw();
  if (!g_live || !g_settings.key || !g_have_table) return;
  *reinterpret_cast<uint32_t*>(kSunGlareWanted) = 0;
  void* const* device =
      reinterpret_cast<void* const*>(bw::render::kDirect3DDevice7);
  if (*device == nullptr) return;
  const int count = KeyRects();
  if (count <= 0) return;
  void* const* slots = *reinterpret_cast<void* const* const*>(*device);
  const ClearFn clear = reinterpret_cast<ClearFn>(slots[kClearSlot]);
  const HRESULT result = clear(*device, static_cast<DWORD>(count),
                               g_key_rects, kClearTarget,
                               g_settings.key_color, 1.0f, 0);
  if (SUCCEEDED(result)) {
    ++g_key_clears;
    g_key_rect_total += static_cast<uint64_t>(count);
  } else if (!g_said_clear_failed) {
    g_said_clear_failed = true;
    ModLog("tabletop     : the clear in the key color failed (0x%08X)",
           static_cast<unsigned>(result));
  }
}

// A LOAD ON THE SAME LAND. A saved game of the land that is up has the same
// block table, so the land does not look new, and the view stayed where it
// was before the load. The game clears the map before each load and each new
// game. So after that, the view is built again at the next main camera call,
// at the game camera of the loaded game, with the last scale and the last
// ground height.
void __fastcall h_ClearMap(void* game, void* unused) {
  o_ClearMap(game, unused);
  if (!g_installed) return;
  g_have_table = false;
  g_build_cause = "a load or a new game";
  g_pair.have = false;
  g_god_zoom = g_god_ground = g_god_turn = 0;
  StopZoomHold("a load or a new game", false);
  StopTurnHold("a load or a new game", false);
  g_tele.pending = false;
  g_spawn.pending = false;
  g_spawn.again = false;
  g_pan = {};
  ++g_clears;
  if (g_clears <= 8) {
    ModLog("%s: the game cleared the map for a load or a new game, so the %s "
           "is built again at the game camera",
           g_settings.god ? "god view     " : "tabletop     ",
           g_settings.god ? "God view" : "table");
  }
}

bool Hook(uintptr_t address, void* replacement, void** original,
          const char* name) {
  void* target = reinterpret_cast<void*>(address);
  MH_STATUS status = MH_CreateHook(target, replacement, original);
  if (status == MH_OK) status = MH_EnableHook(target);
  ModLog("hooks        : %s at 0x%08X %s", name, address,
         status == MH_OK ? "ok" : "FAILED");
  return status == MH_OK;
}

void SetFloat(Patch& p, float value) { p.wanted = Bits(value); }

// What god_zoom_about is now, for the start of the log and for the test key.
void LogZoomAbout(const char* tag) {
  const int about = g_settings.god_zoom_about;
  char what[160];
  if (about == zoom_math::kAboutLook) {
    sprintf_s(what, "the land point that you look at, at most %d cm from "
              "your head (god_zoom_about_far_cm)",
              g_settings.god_zoom_about_far_cm);
  } else if (about == zoom_math::kAboutHand) {
    sprintf_s(what, "the Hand's point");
  } else {
    sprintf_s(what, "the land point under your head");
  }
  ModLog("%s: god_zoom_about=%d. A zoom keeps %s at its place in the room%s",
         tag, about, what,
         g_settings.test_keys ? ". Ctrl, Shift and Delete change it" : "");
}

// The rule of the walk with the stick, for the log of the start and of the
// test key.
void LogMoveStick(const char* tag) {
  const int rule = walk_math::Rule(g_settings.move_stick);
  const char* keys =
      g_settings.test_keys ? ". Shift and the Menu key change it" : "";
  if (rule == walk_math::kRuleOff) {
    ModLog("%s: move_stick=0. The stick of the hand that does not point does "
           "not move you%s", tag, keys);
    return;
  }
  ModLog("%s: move_stick=%d. While the stick of the hand that does not point "
         "is pushed, you %s. Forward is %s, and a full push moves you %d cm "
         "of the room each second (move_stick_speed_cm). %s. %s%s",
         tag, rule,
         Flies() ? "fly along that direction in 3D" : "move over the land",
         WalkRuleText(rule), g_settings.move_stick_speed_cm, FollowText(),
         RampText(), keys);
}

}  // namespace

void TabletopSetOptions(const TabletopSettings& settings) {
  g_settings = settings;
  if (g_settings.diameter_cm < 20) g_settings.diameter_cm = 20;
  if (g_settings.margin_pct < 0) g_settings.margin_pct = 0;
  if (g_settings.units_per_meter < 0) g_settings.units_per_meter = 0;
  if (g_settings.hand_cm < 2) g_settings.hand_cm = 2;
  if (g_settings.hand_cm > 40) g_settings.hand_cm = 40;
  // The God view has no edge, so it has no key color either.
  if (g_settings.god) g_settings.key = false;
  if (g_settings.god_down_cm < 10) g_settings.god_down_cm = 10;
  if (g_settings.god_down_cm > 200) g_settings.god_down_cm = 200;
  if (g_settings.god_units_per_meter < 1) g_settings.god_units_per_meter = 1;
  if (g_settings.god_zoom_pct < 5) g_settings.god_zoom_pct = 5;
  if (g_settings.god_zoom_pct > 300) g_settings.god_zoom_pct = 300;
  g_settings.god_zoom_smooth = g_settings.god_zoom_smooth != 0 ? 1 : 0;
  if (g_settings.god_zoom_smooth_ms < 100) g_settings.god_zoom_smooth_ms = 100;
  if (g_settings.god_zoom_smooth_ms > 5000) {
    g_settings.god_zoom_smooth_ms = 5000;
  }
  g_settings.god_zoom_about = zoom_math::About(g_settings.god_zoom_about);
  if (g_settings.god_zoom_about_far_cm < 50) {
    g_settings.god_zoom_about_far_cm = 50;
  }
  if (g_settings.god_zoom_about_far_cm > 1000) {
    g_settings.god_zoom_about_far_cm = 1000;
  }
  if (g_settings.god_step_cm < 1) g_settings.god_step_cm = 1;
  if (g_settings.god_step_cm > 50) g_settings.god_step_cm = 50;
  // At 8000 units per meter an arm of 70 cm is 5600 units, the reach of the
  // Hand (kReach).
  if (g_settings.god_most_units < 10) g_settings.god_most_units = 10;
  if (g_settings.god_most_units > 8000) g_settings.god_most_units = 8000;
  // 0 is the life size scale. 1 is a tenth of a unit for each meter, where
  // a villager is 16 m high.
  if (g_settings.god_least_tenths < 0) g_settings.god_least_tenths = 0;
  if (g_settings.god_zoom_solid_cm < 5) g_settings.god_zoom_solid_cm = 5;
  if (g_settings.god_zoom_solid_cm > 200) g_settings.god_zoom_solid_cm = 200;
  if (g_settings.god_least_tenths > 10 * g_settings.god_most_units) {
    g_settings.god_least_tenths = 10 * g_settings.god_most_units;
  }
  g_settings.god_spawn = g_settings.god_spawn != 0 ? 1 : 0;
  if (g_settings.god_spawn_ahead_cm < 0) g_settings.god_spawn_ahead_cm = 0;
  if (g_settings.god_spawn_ahead_cm > 150) g_settings.god_spawn_ahead_cm = 150;
  g_settings.move_stick = walk_math::Rule(g_settings.move_stick);
  if (g_settings.move_stick_speed_cm < 10) g_settings.move_stick_speed_cm = 10;
  if (g_settings.move_stick_speed_cm > 500) {
    g_settings.move_stick_speed_cm = 500;
  }
  g_settings.move_stick_follow_land =
      g_settings.move_stick_follow_land != 0 ? 1 : 0;
  g_settings.move_type = walk_math::Type(g_settings.move_type);
  g_settings.move_stick_ramp = g_settings.move_stick_ramp != 0 ? 1 : 0;
  g_settings.fly_solid_slide = g_settings.fly_solid_slide != 0 ? 1 : 0;
  if (g_settings.move_stick_ramp_most < 1) g_settings.move_stick_ramp_most = 1;
  if (g_settings.move_stick_ramp_most > 64) {
    g_settings.move_stick_ramp_most = 64;
  }
  if (g_settings.move_stick_trace < 0) g_settings.move_stick_trace = 0;
  if (g_settings.move_stick_trace > 2000) g_settings.move_stick_trace = 2000;
  // The speed by the scale: ref 1 to 4000 units per meter, the power 0 to
  // 1, the least factor 10 to 100 percent, the largest 100 to 1000.
  g_settings.move_stick_scale = g_settings.move_stick_scale != 0 ? 1 : 0;
  int& ref = g_settings.move_stick_scale_ref_x10;
  if (ref < 10) ref = 10;
  if (ref > 40000) ref = 40000;
  int& power = g_settings.move_stick_scale_power_pct;
  if (power < 0) power = 0;
  if (power > 100) power = 100;
  int& low = g_settings.move_stick_scale_low_pct;
  if (low < 10) low = 10;
  if (low > 100) low = 100;
  int& high = g_settings.move_stick_scale_high_pct;
  if (high < 100) high = 100;
  if (high > 1000) high = 1000;
}

// Only when play_mode picks the table. With any other play mode, nothing in
// this file is hooked and nothing is written.
void TabletopInstall() {
  if (g_installed) return;
  if (!HeadCameraEnabled()) {
    ModLog("tabletop     : the table needs head_camera=1, so it stays off");
    return;
  }
  SetFloat(g_vanish, kObjectsFar);
  SetFloat(g_size_cap, kSizeCapFar);
  SetFloat(g_detail_near, kFar);
  SetFloat(g_detail_far, kFar);
  SetFloat(g_texture_line, kFar);
  SetFloat(g_reach, kReach);
  g_fog.wanted = 0;
  g_reflection.wanted = 0;
  for (Patch* p : g_patches) {
    p->usable = !IsBadWritePtr(reinterpret_cast<void*>(p->address),
                               sizeof(uint32_t));
    if (!p->usable) {
      ModLog("tabletop     : 0x%08X is not writable, left alone", p->address);
    }
  }
  g_texture_line.usable = g_texture_line.usable && g_settings.full_textures;

  // The bracket is what keeps the game logic on its own camera, so without
  // it the table must not start.
  if (!FrameTimersGraphicsHooked()) {
    ModLog("tabletop     : GGame::ProcessGraphicsEngine is not hooked, and "
           "without the render bracket the table stays off");
    return;
  }
  // Both the table and the God view are built again after a load.
  Hook(kClearMap, reinterpret_cast<void*>(&h_ClearMap),
       reinterpret_cast<void**>(&o_ClearMap), "GGame::ClearMap");
  // THE GOD VIEW has no edge: no block cull, no sky hook and no sea disc.
  // The near plane is in near_plane.cpp.
  if (g_settings.god) {
    g_installed = true;
    g_on = true;
    ModLog("god view     : on. The ground is %d cm below your head, the view "
           "starts at %d units per meter, and it zooms from %.1f to %.0f units "
           "per meter, %d%% a step. The ground goes %d cm a step. Ctrl F11 "
           "turns it off and on.", g_settings.god_down_cm,
           g_settings.god_units_per_meter, GodLowest(), GodHighest(),
           g_settings.god_zoom_pct, g_settings.god_step_cm);
    if (g_settings.god_spawn != 0) {
      ModLog("god view     : god_spawn=1. When the game puts its camera at a "
             "new place, you go there: the land point that it looks at is "
             "%d cm in front of your head (god_spawn_ahead_cm)%s",
             g_settings.god_spawn_ahead_cm,
             g_settings.test_keys ? ". Ctrl and Numpad 0 change it" : "");
    } else {
      ModLog("god view     : god_spawn=0. The view does not follow the game "
             "camera%s",
             g_settings.test_keys ? ". Ctrl and Numpad 0 change it" : "");
    }
    if (g_settings.god_zoom_smooth != 0) {
      ModLog("god view     : god_zoom_smooth=1. The stick zooms while it is "
             "held, and a full push changes the scale %.2f times in %d ms "
             "(god_zoom_smooth_ms)%s",
             1.0f + g_settings.god_zoom_pct * 0.01f,
             g_settings.god_zoom_smooth_ms,
             g_settings.test_keys ? ". Ctrl, Shift and Insert change it" : "");
    } else {
      ModLog("god view     : god_zoom_smooth=0. One push of the stick is one "
             "zoom step%s",
             g_settings.test_keys ? ". Ctrl, Shift and Insert change it" : "");
    }
    LogZoomAbout("god view     ");
    LogMoveStick("god view     ");
    CameraZoneInstall(g_settings.god_zone, g_settings.god_zone_margin);
    return;
  }
  Hook(kBlockCull, reinterpret_cast<void*>(&h_BlockCull),
       reinterpret_cast<void**>(&o_BlockCull), "the land block cull");
  Hook(kSkyDraw, reinterpret_cast<void*>(&h_SkyDraw),
       reinterpret_cast<void**>(&o_SkyDraw), "LH3DSky::Draw");
  g_sea_patched = PatchSeaCall();
  ModLog("tabletop     : the plain sea %s",
         g_sea_patched ? "becomes a disc at the edge of the table"
                       : "call is not the expected one, left alone");
  g_installed = true;
  g_on = true;
  ModLog("tabletop     : on. The table is %d cm across, %d cm ahead of you and "
         "%d cm below your head. Ctrl F11 turns it off and on.",
         g_settings.diameter_cm, g_settings.forward_cm, g_settings.down_cm);
  ModLog("tabletop     : a controller's Hand is %d cm long on the table. The "
         "grab of the world moves the land on the table, and its zoom stops "
         "at %.0f units per meter.", g_settings.hand_cm, kLowestScale);
  if (g_settings.units_per_meter > 0) {
    ModLog("tabletop     : the scale is set to %d units per meter",
           g_settings.units_per_meter);
  }
  if (g_settings.key) {
    ModLog("tabletop     : past the table is the key color %06X",
           static_cast<unsigned>(g_settings.key_color & 0xFFFFFF));
  }
}

// Ctrl F11, the step of table_sweep and the debug page. The table faces the
// game camera again when it comes back. `by` must be a string that stays.
void TabletopChange(const char* by) {
  if (!g_installed) return;
  g_on = !g_on;
  g_have_table = false;
  g_build_cause = by;
  g_owned = false;
  g_god_zoom = g_god_ground = g_god_turn = 0;
  StopZoomHold(by, false);
  StopWalk(by, false);
  StopTurnHold(by, false);
  g_spawn.pending = false;
  g_spawn.again = false;
  g_pan = {};
  if (g_settings.god) {
    ModLog("god view     : the God view is now %s", g_on ? "on" : "off");
  } else {
    ModLog("tabletop     : the table is now %s", g_on ? "on" : "off");
  }
}

void TabletopToggle() { TabletopChange("Ctrl F11"); }

// The switch of the debug page: 1 while the table or the God view is on.
int TabletopValue() {
  if (!g_installed) return kDebugCannot;
  return g_on ? 1 : 0;
}

// THE MAIN CAMERA CALL OF A FRAME, from head_camera.cpp. True when the table
// takes this frame. The camera to use is then (position_out, focus_out), and
// the lean is multiplied by lean_gain. The game keeps the camera it asked
// for. head_camera.cpp calls this once a frame.
bool TabletopCamera(const float asked_position[3], const float asked_focus[3],
                    float position_out[3], float focus_out[3],
                    float* lean_gain) {
  g_live = false;
  g_owned = false;
  if (!g_installed || !g_on) return false;
  // The Temple and the other rooms draw from a camera of their own.
  if (!GameCameraPlain()) {
    if (!g_said_aside) {
      g_said_aside = true;
      ModLog("%s: the game draws from another camera, so the %s stands aside",
             g_settings.god ? "god view     " : "tabletop     ",
             g_settings.god ? "God view" : "table");
    }
    StopWalk("the game draws from another camera", false);
    StopTurnHold("the game draws from another camera", false);
    return false;
  }
  g_said_aside = false;
  if (g_settings.god) {
    // The land drag of this frame, read once, so that it cannot go stale.
    const bool drag = ComfortCameraTakeGodDrag();
    // A STORY HOLDS THE CAMERA: the God view stands aside, and the story plays
    // as the game made it, at the life size scale. A story frames its shots
    // for the game camera, and the God view would not show them (the same
    // risk as the table's, docs/tabletop.md, "Known limits and open questions"). When the
    // player's camera is back, the God view is built again where the story
    // left the camera.
    //
    // THE CUT SCENE WINDOW (cutscene_window.cpp) is the same case: in a flat
    // frame the game camera goes to a screen in the room, and head_camera.cpp
    // makes this call only so that the God view lets go of the frame.
    const bool window = CutsceneWindowFlat();
    if (ScriptCameraStoryOwns() || window) {
      if (!g_god_story) {
        g_god_story = true;
        ModLog("god view     : %s, so the God view stands aside until your "
               "camera is back",
               window ? "a cut scene plays in its window"
                      : "a story holds the camera");
      }
      g_have_table = false;
      g_build_cause = window ? "the end of a cut scene" : "the end of a story";
      g_god_zoom = g_god_ground = g_god_turn = 0;
      StopZoomHold("the God view stands aside", false);
      StopWalk("the God view stands aside", false);
      StopTurnHold("the God view stands aside", false);
      g_motion.have = false;
      g_tele.pending = false;
      g_spawn.pending = false;
      g_spawn.again = false;
      g_pan = {};
      return false;
    }
    if (g_god_story) {
      g_god_story = false;
      ModLog("god view     : your camera is back, so the God view is too");
    }
    if (!GodLand()) return false;
    CameraZoneWatch();
    // The time of this frame, for the smooth zoom and the walk. With
    // god_display_time=1 it is the time between the display times of two
    // frames of the headset, so each frame moves the view by the time
    // between two pictures. The time of the CPU between two calls changes by
    // some milliseconds from frame to frame, and a fast walk then judders.
    const double clock = Seconds();
    double frame_seconds = g_god_clock > 0.0 ? clock - g_god_clock : 0.0;
    g_god_clock = clock;
    // The time of the CPU, for the log of the smooth turn.
    const double cpu_seconds = frame_seconds;
    const double display = ModDisplaySeconds();
    if (g_settings.god_display_time != 0 && display > 0.0 &&
        g_god_display > 0.0) {
      const double between = display - g_god_display;
      // A display time that did not move, or a gap of a stop, keeps the
      // time of the CPU.
      if (between > 0.0 && between < 0.1) {
        frame_seconds = between;
        ++g_display_frames;
      }
    }
    g_god_display = display;
    // The test god_zoom_test_toggle: the test key acts at one frame.
    if (g_settings.god_zoom_test_toggle > 0 &&
        ControllersFrameCount() ==
            static_cast<uint64_t>(g_settings.god_zoom_test_toggle)) {
      static bool done = false;
      if (!done) {
        done = true;
        TabletopGodZoomSmoothToggle();
      }
    }
    // The test god_zoom_about_test: the test key acts at one frame.
    if (g_settings.god_zoom_about_test > 0 &&
        ControllersFrameCount() ==
            static_cast<uint64_t>(g_settings.god_zoom_about_test)) {
      static bool done = false;
      if (!done) {
        done = true;
        TabletopGodZoomAboutCycle();
      }
    }
    if (g_zoom_probe.armed) {
      g_zoom_probe.armed = false;
      float eye[3];
      if (HeadCameraTableEye(eye) && ZoomLog()) {
        const float dx = eye[0] - g_zoom_probe.eye[0];
        const float dz = eye[2] - g_zoom_probe.eye[2];
        // The point of the zoom from the drawn head, in the room, before and
        // after the step.
        float off = 0.0f;
        for (int i = 0; i < 3; ++i) {
          const float was = (g_zoom_probe.pivot[i] - g_zoom_probe.eye[i]) /
                            g_zoom_probe.before;
          const float is = (g_zoom_probe.pivot[i] - eye[i]) /
                           g_zoom_probe.scale;
          off += (is - was) * (is - was);
        }
        ModLog("god view     : zoom check, the drawn head moved %.2f cm of the "
               "room over the land in the step (%.1f %.1f before, %.1f %.1f "
               "after). The point of the zoom moved %.2f cm from the drawn "
               "head in the room.",
               sqrtf(dx * dx + dz * dz) / g_zoom_probe.scale * 100.0f,
               g_zoom_probe.eye[0], g_zoom_probe.eye[2], eye[0], eye[2],
               sqrtf(off) * 100.0f);
        // NEAR THE LOWEST SCALE: what the eyes get after the step. The
        // height of the eye over the land, the near plane, the fog of the
        // game and the overlay of the head blinder. One of them can hide the
        // land at a small scale.
        if (g_zoom_probe.scale < 2.0f * HeadPoseScale()) {
          const char* what = "";
          const float clearance = HeadBlinderClearance(&what);
          const float near_plane =
              *reinterpret_cast<const float*>(bw::tech::kNearPlane);
          ModLog("god view     : zoom check, the scene at %.2f units per "
                 "meter: the eye is %.2f units over the land, the near "
                 "plane is %.3f units (%.1f cm of the room), the fog of the "
                 "game is %s from %.0f to %.0f units, and the head blinder "
                 "covers %.0f%% (your head is %.1f cm from %s)",
                 g_zoom_probe.scale, eye[1] - GroundAt(eye[0], eye[2]),
                 near_plane, near_plane / g_zoom_probe.scale * 100.0f,
                 *reinterpret_cast<const int32_t*>(kFog) != 0 ? "on" : "off",
                 *reinterpret_cast<const float*>(kFogStart),
                 *reinterpret_cast<const float*>(kFogEnd),
                 HeadBlinderLevel() * 100.0f, clearance * 100.0f, what);
        }
      }
    }
    if (!g_have_table) {
      float position[3], focus[3];
      GameCameraNow(asked_position, asked_focus, position, focus);
      if (!PlaceGod(g_build_cause, position, focus, true)) return false;
      g_spawn.pending = false;
      g_spawn.again = false;
      g_motion.have = false;
    }
    // THE COMFORT VIGNETTE: what a grab of the world moved since the last
    // main camera call (TabletopMove).
    if (g_motion.have && ControllersGrabHeld()) {
      VignetteMove(MotionShare(g_motion.pose, Pose(), frame_seconds),
                   "the grab of the world");
    }
    // A move of the game camera to a new place, once, as one cut.
    ApplyGodSpawn(asked_position, asked_focus);
    // The steps of the player, the smooth zoom, the walk with the stick and
    // the smooth turn, once a frame, before anything reads the view of this
    // frame. Then the pan of the mouse. The steps tell the vignette by
    // themselves, and the smooth zoom, the walk and the smooth turn give it
    // the share of their move.
    ApplyGodSteps();
    const TablePose before_moves = Pose();
    ApplyGodZoomHold(frame_seconds);
    ApplyGodWalk(frame_seconds);
    ApplyGodTurnHold(frame_seconds, cpu_seconds);
    if (g_hold.active || g_walk.active || g_turn_hold.active) {
      const bool flies = g_walk.active && g_walk.type == walk_math::kTypeFly;
      VignetteMove(MotionShare(before_moves, Pose(), frame_seconds, flies),
                   flies           ? "the fly with the stick"
                   : g_walk.active ? "the walk with the stick"
                   : g_hold.active ? "the smooth zoom"
                                   : "the smooth turn");
    }
    MousePan(drag);
    g_motion.pose = Pose();
    g_motion.have = true;
  } else {
    if (!FitLand()) return false;
    if (!g_have_table) {
      float heading = 0.0f;
      if (!Heading(asked_position, asked_focus, &heading)) return false;
      BuildTable(heading);
    }
    ApplyTableTeleport();
  }
  for (int i = 0; i < 3; ++i) {
    position_out[i] = g_origin[i];
    focus_out[i] = g_origin[i] + g_basis[i * 3 + 2] * g_focus_distance;
  }
  *lean_gain = LeanGain(g_scale, HeadPoseScale());
  g_live = true;
  g_owned = true;
  return true;
}

// The table scale while the table owns the frame, and 0 otherwise.
float TabletopScale() { return g_live ? g_scale : 0.0f; }

bool TabletopOwnsView() {
  return g_installed && g_on && g_owned && g_have_table;
}

float TabletopViewScale() { return TabletopOwnsView() ? g_scale : 0.0f; }

// For head_blinder.cpp: how far from the head, in meters of the room, the
// zoom wants the solids. Below twice the life size scale a zoom step can
// bring a house to the head, and kZoomSolidReachM holds the houses that the
// next step can bring. 0 when the zoom does not look at the solids.
float TabletopZoomSolidReachM() {
  if (!g_settings.god || g_settings.god_zoom_solid_stop == 0 ||
      !TabletopOwnsView()) {
    return 0.0f;
  }
  // A fly moves the head over the land at each scale, and a step of the
  // speed ramp is up to 80 cm of the room.
  if (g_walk.active && g_walk.type == walk_math::kTypeFly) {
    return kZoomSolidReachM;
  }
  return g_scale < 2.0f * HeadPoseScale() ? kZoomSolidReachM : 0.0f;
}

// THE ROOM OF THE TABLE, for the hands and the spirits. The same numbers the
// main camera call of this frame used, so the Hand and the picture agree.
bool TabletopRoom(float origin[3], float basis[9], float center[3],
                  float* gain) {
  if (!g_live || !g_have_table) return false;
  for (int i = 0; i < 3; ++i) {
    origin[i] = g_origin[i];
    center[i] = g_center[i];
  }
  for (int i = 0; i < 9; ++i) basis[i] = g_basis[i];
  *gain = LeanGain(g_scale, HeadPoseScale());
  return true;
}

// THE HAND ON THE TABLE. table_hand_cm (10) is the Hand's length in the room.
// That is half of a real hand, so the Hand does not hide the village under
// it. The controller's grip is about 5 cm over the table when the fingers
// touch it, so the Hand then rests just over the land. hand_direct.cpp lets
// the point come down to 5 mm over the land (docs/hand.md, "The touch of the
// land"). The grab radius of hand_direct.cpp, 0.6 of the length, is 6 cm.
//
// IN THE GOD VIEW the Hand is the real hand: hand_real_cm (19) times
// hand_scale_pct, at the scale. So it is the size of the player's own
// hand in the room at every zoom, and large against the villagers when the
// view is zoomed out.
float TabletopHandSize() {
  if (!TabletopOwnsView()) return 0.0f;
  if (g_settings.god) {
    const float length =
        ControllersHandCm() * 0.01f * ControllersHandScalePct() * 0.01f;
    if (!(length > 0.0f)) return 0.0f;
    const float size = HandSize(length, g_scale);
    if (!g_said_hand) {
      g_said_hand = true;
      ModLog("god view     : the Hand is your real hand, %.1f cm long, which "
             "is size %.2f at %.1f units per meter", length * 100.0f, size,
             g_scale);
    }
    return size;
  }
  if (g_settings.hand_cm <= 0) return 0.0f;
  const float size = HandSize(g_settings.hand_cm * 0.01f, g_scale);
  if (!g_said_hand) {
    g_said_hand = true;
    ModLog("tabletop     : the Hand is %d cm long on the table, which is size "
           "%.1f at %.0f units per meter", g_settings.hand_cm, size, g_scale);
  }
  return size;
}

bool TabletopPose(TablePose* pose, float table_room[3], float* lowest_scale,
                  float* highest_scale, uint64_t* serial) {
  if (!TabletopOwnsView()) return false;
  for (int i = 0; i < 3; ++i) pose->center[i] = g_center[i];
  pose->heading = g_heading;
  pose->scale = g_scale;
  TableRoom(table_room);
  if (g_settings.god) {
    *lowest_scale = GodLowest();
    *highest_scale = GodHighest();
  } else {
    *highest_scale = g_fit_scale > kLowestScale ? g_fit_scale : kLowestScale;
    *lowest_scale = kLowestScale;
  }
  *serial = g_serial;
  return true;
}

// THE GRAB MOVES THE LAND ON THE TABLE. The table top stays in the room.
// The center stays on the map, and the sea disc and the edge follow. In the
// God view the height of the waist point stays as the grab gives it, which
// is the height at the press, so a pan never lifts or drops the view.
//
// The game's switch for the drag of the land (bit 8) also holds for the grab.
// While it is off, the grab moves nothing, and the serial goes up, so the
// grab anchors again on each frame. So when the switch comes on, the land
// does not jump to where the hands went in the meantime.
// TabletopGrabAllows gives the switches for the turn and the zoom. A move
// sends the events of the camera help, as the game's own drag, turn and zoom
// do.
void TabletopMove(const TablePose& pose) {
  if (!g_installed || !g_have_table) return;
  if (!(pose.scale > 0.0f) || !(pose.heading == pose.heading)) return;
  if ((CameraFeatures() & kFeatureDrag) == 0) {
    ++g_serial;
    ++g_switch_refused;
    static int said = 0;
    if (said < 4) {
      ++said;
      ModLog("%s: the game does not allow a drag of the land now (camera "
             "switch 8 is off), so the grab moves nothing",
             g_settings.god ? "god view     " : "tabletop     ");
    }
    return;
  }
  const TablePose before = Pose();
  float place[3] = {pose.center[0], 0.0f, pose.center[2]};
  for (int i = 0; i < 3; i += 2) {
    if (!(place[i] >= 0.0f)) place[i] = 0.0f;
    if (place[i] > kMapSize) place[i] = kMapSize;
  }
  // In the God view the grab keeps to the camera zone of the game. At the
  // limit the land stops, and the hand keeps moving.
  if (g_settings.god) {
    CameraZoneLimitMove(g_center, &place[0], &place[2], "the grab");
  }
  g_center[0] = place[0];
  g_center[2] = place[2];
  g_center[1] = 0.0f;
  if (g_settings.god && pose.center[1] == pose.center[1]) {
    g_center[1] = pose.center[1];
  }
  g_heading = Wrap(pose.heading);
  g_scale = pose.scale;
  Derive();
  ++g_moves;
  if (g_settings.god) {
    ++g_god_version;
    g_god_nonturn_version = g_god_version;
  }
  MoveHelp(before, kInputButton);
}

// THE GROUND LIMITS OF THE GRAB (docs/god-view.md, "The ground height" and
// "Moving around: the grab and the mouse"). In the
// God view the grab of one hand also moves the land up and down. This cuts
// the height the solve asked for, so the land under the head stays 10 cm to
// 2.5 m below the head. At a limit the land stops, and it never jumps
// (GodGroundHeight). The land is looked up under the head in the pose that
// the solve made, so a pan over a hill is judged where the head will be.
// False when the God view does not own the view. cut is true when the limit
// held the height back.
bool TabletopGodLimitHeight(TablePose* pose, bool* cut) {
  if (cut != nullptr) *cut = false;
  if (!g_installed || !g_have_table || !g_settings.god) return false;
  float room[3], head[3];
  TableRoom(room);
  HeadRoom(head);
  TablePose at_pose = *pose;
  for (int i = 0; i < 3; i += 2) at_pose.center[i] = ClampMap(pose->center[i]);
  float at[3];
  PoseWorld(at_pose, room, head, at);
  const float land = GroundAt(at[0], at[2]);
  const float wanted = pose->center[1];
  const float limited =
      GodGroundHeight(wanted, g_center[1], land, room[1], pose->scale,
                      FloorHeadY(head), kGroundNearest, kGroundFarthest);
  pose->center[1] = limited;
  if (cut != nullptr) *cut = limited != wanted;
  return true;
}

// For the gain of the grab (world_grab.cpp): how far the land is below a
// point of the room (meters, in the room of the table), in the pose of now.
// False when the God view does not own the view.
bool TabletopGodLandBelow(const float point[3], float* below_m) {
  if (!g_installed || !g_have_table || !g_settings.god) return false;
  float room[3];
  TableRoom(room);
  const TablePose pose = Pose();
  if (!(pose.scale > 0.0f)) return false;
  float at[3];
  PoseWorld(pose, room, point, at);
  *below_m = point[1] - GroundInRoom(pose, room,
                                     GroundAt(ClampMap(at[0]), ClampMap(at[2])));
  return true;
}

// THE GAME'S SWITCHES FOR THE GRAB. A two hand grab turns the land only when
// the game lets the camera turn (bit 2), and zooms only when it lets the
// camera zoom (bit 4). world_grab.cpp passes these to the solve, which then
// keeps the last turn and the last zoom, so nothing jumps.
void TabletopGrabAllows(bool* turn, bool* zoom) {
  const uint32_t features = CameraFeatures();
  if ((features & kFeatureTurn) == 0) *turn = false;
  if ((features & kFeatureZoom) == 0) *zoom = false;
}

// THE EARS ON THE TABLE (docs/tabletop.md, "The ears on the table"). The render head is 800 to
// 3200 units from the land, and the game plays almost nothing that far from
// the ears. So the ears go straight down from the head to at most 400 units
// over the land under it, and stay inside the edge of the table. They turn
// with the head. A head that is lower than that is the ears itself.
//
// The God view has no edge, so the ears go straight down from the head. At
// the life size scale the head is less than 400 units over the land, and the
// ears are the head.
bool TabletopEars(const float head[3], float out[3]) {
  if (!TabletopOwnsView()) return false;
  if (g_settings.god) {
    for (int i = 0; i < 3; ++i) out[i] = head[i];
  } else {
    InsideEdge(head, g_center, g_radius, out);
  }
  float x = out[0], z = out[2];
  if (x < 0.0f) x = 0.0f;
  if (x > kMapSize - 1.0f) x = kMapSize - 1.0f;
  if (z < 0.0f) z = 0.0f;
  if (z > kMapSize - 1.0f) z = kMapSize - 1.0f;
  float ground = Altitude(x, z);
  if (!(ground >= 0.0f && ground < 1000.0f)) ground = 0.0f;
  out[1] = EarsHeight(head[1], ground, kEarsAbove);
  if (!g_said_ears) {
    g_said_ears = true;
    ModLog("%s: the ears are over the land under your head, %.0f units over "
           "it, and your head is %.0f units over it",
           g_settings.god ? "god view     " : "tabletop     ", out[1] - ground,
           head[1] - ground);
  }
  return true;
}

// ONCE A FRAME, before the eye passes. The values of the engine at the table
// scale follow the
// table: set while it owns the frame, and the game's own values otherwise.
//
// IN THE GOD VIEW they follow the scale. The distances (the map objects, the
// model detail cap, the land detail and texture lines and the Hand's reach)
// are the game's own values times the gain s / 7.5, up to the table values.
// So at the life size scale the game keeps its own values, and a distance in
// the room stays the same at every zoom. The fog and the reflection stay the
// game's below the gain kGodFarGain, and go off above it, as on the table.
//
// THE CULL DISTANCE (cull_distance.cpp) multiplies the map object distance
// on top, in each frame, also in the frames that the view does not own. On
// the table the distance is kObjectsFar while the table owns the frame. The
// fog moves out by the ratio of the distance with the cull factor to the
// distance without it, so where the cap holds both the fog does not change.
void TabletopApplyFrame() {
  if (!g_installed) return;
  ++g_frames;
  if (g_live) ++g_live_frames;
  CullDistanceFrame();
  const float cull = CullDistanceFactor();
  const float life = HeadPoseScale();
  const float gain = g_live && life > 0.0f ? g_scale / life : 1.0f;
  if (g_settings.god) {
    const bool zoomed_out = g_live && gain >= kGodFarGain;
    ApplyPatch(g_fog, zoomed_out);
    ApplyPatch(g_reflection, zoomed_out);
    const float before = cull_math::VanishGain(g_live, gain, 1.0f);
    const float after = cull_math::VanishGain(g_live, gain, cull);
    ApplyScaled(g_vanish, true, after, kObjectsFar);
    CullDistanceSetFog(cull_math::FogRatio(VanishGame(), before, after,
                                           kObjectsFar));
    ApplyScaled(g_size_cap, g_live, gain, kSizeCapFar);
    ApplyScaled(g_detail_near, g_live, gain, kFar);
    ApplyScaled(g_detail_far, g_live, gain, kFar);
    ApplyScaled(g_texture_line, g_live, gain, kFar);
  } else {
    for (Patch* p : g_patches) {
      if (p != &g_vanish) ApplyPatch(*p, g_live);
    }
    const float before = g_live ? kFar : 1.0f;
    const float after = g_live ? kFar : cull;
    ApplyScaled(g_vanish, true, after, kObjectsFar);
    CullDistanceSetFog(cull_math::FogRatio(VanishGame(), before, after,
                                           kObjectsFar));
  }

  // The Hand's reach is a field of the CHand, and a script can set it back.
  uint8_t* hand = Hand();
  const uintptr_t reach =
      hand != nullptr ? reinterpret_cast<uintptr_t>(hand + kHandReach) : 0;
  if (reach != g_reach.address) {
    // Another CHand, or none. The old one may be gone, so it is not touched.
    g_reach.address = reach;
    g_reach.have = false;
  }
  if (reach != 0 && !IsBadWritePtr(reinterpret_cast<void*>(reach),
                                   sizeof(float))) {
    if (g_settings.god) {
      ApplyScaled(g_reach, g_live, gain, kReach);
    } else {
      ApplyPatch(g_reach, g_live);
    }
  }

  if (g_live && !g_said_depth) {
    g_said_depth = true;
    const int32_t depth16 = *reinterpret_cast<const int32_t*>(kZBuffer16);
    ModLog("tabletop     : the Z buffer is %s bit. The near plane is %.2f "
           "units.", depth16 == 1 ? "16" : "24 or 32",
           *reinterpret_cast<const float*>(bw::tech::kNearPlane));
  }
}

// THE BRACKET ENDS, after GGame::ProcessGraphicsEngine. The picture, the
// Hand and the interface are done, so the game logic gets its own camera
// back: the matrices and the stored camera. frame_timers.cpp calls this.
void TabletopGraphicsDone() {
  if (g_live && HeadCameraUseGameCamera()) ++g_bracket_ends;
  g_live = false;
}

// Clouds and ground mist past the edge are not drawn. sky_detail.cpp asks
// before it draws each dome. With the key color on, no cloud is drawn at
// all: a cloud is not opaque, and the key color shows through the clouds
// that reach past the edge.
bool TabletopHidesMist(const void* mist) {
  if (!g_live || !g_have_table || mist == nullptr) return false;
  // The God view has no edge, and every cloud draws.
  if (g_settings.god) return false;
  if (g_settings.key) {
    ++g_mist_hidden;
    return true;
  }
  const float* p = reinterpret_cast<const float*>(
      static_cast<const uint8_t*>(mist) + kMistPosition);
  const float dx = p[0] - g_center[0], dz = p[2] - g_center[2];
  if (dx * dx + dz * dz <= g_radius * g_radius) return false;
  ++g_mist_hidden;
  return true;
}

// Once in each eye pass, before LH3DRender::FinishFrame. The two lists that
// stop without a message are read here, with the table on and off. The game
// fills the list of map objects from the blocks near to far, and a full list
// takes no more. So a full list loses the far objects, never the near ones.
void TabletopCountLists() {
  if (!g_installed) return;
  const int32_t objects = *reinterpret_cast<const int32_t*>(kObjectCount);
  const int32_t sorted = *reinterpret_cast<const int32_t*>(kSortedCount);
  if (objects > g_most_objects) g_most_objects = objects;
  if (sorted > g_most_sorted) g_most_sorted = sorted;
  const bool objects_full = objects >= kObjectLimit;
  const bool sorted_full = sorted >= kSortedLimit;
  if (objects_full) ++g_objects_full;
  if (sorted_full) ++g_sorted_full;
  if ((objects_full || sorted_full) && g_full_logs < 8) {
    ++g_full_logs;
    ModLog("tabletop     : a list is full with the table %s: %d of %d map "
           "objects, %d of %d sorted objects. The rest are not drawn. Map "
           "objects stop at %.0f units (cull factor %.0f).",
           g_live ? "on" : "off", objects, kObjectLimit, sorted,
           kSortedLimit, *reinterpret_cast<const float*>(kVanish),
           CullDistanceFactor());
  }
}

namespace {

// The report of the God view.
void GodReport() {
  ModLog("god view     : the God view owned %llu of %llu frames, and gave the "
         "game its camera back %llu times", g_live_frames, g_frames,
         g_bracket_ends);
  ModLog("god view     : at most %d of %d map objects and %d of %d sorted "
         "objects in one pass. Full in %llu and %llu passes.",
         g_most_objects, kObjectLimit, g_most_sorted, kSortedLimit,
         g_objects_full, g_sorted_full);
  if (g_reach.have) {
    ModLog("god view     : the Hand reaches %.0f units, the game set %.0f",
           FloatOf(g_reach.written), FloatOf(g_reach.original));
  }
  if (g_vanish.have || g_fog.have) {
    ModLog("god view     : map objects stop at %.0f units (the game set %.0f), "
           "the fog is %s", FloatOf(g_vanish.have ? g_vanish.written
                                                  : g_vanish.original),
           FloatOf(g_vanish.original), g_fog.have ? "off" : "the game's");
  }
  if (g_have_table) {
    float room[3], head[3], at[3];
    TableRoom(room);
    HeadRoom(head);
    PoseWorld(Pose(), room, head, at);
    const float land = GroundAt(at[0], at[2]);
    ModLog("god view     : the waist point is at %.1f %.1f %.1f, heading %.2f "
           "degrees, %.1f units per meter. The land under your head is %.2f m "
           "below it. %llu steps, %llu refused, and the grab moved the view on "
           "%llu frames. The scripts of the game got the player in the place "
           "of the camera %llu times.", g_center[0], g_center[1], g_center[2],
           g_heading * 57.29578f, g_scale,
           head[1] - GroundInRoom(Pose(), room, land), g_god_steps,
           g_god_refused, g_moves, HandDirectTakeScriptAnswers());
  }
  if (g_pans != 0 || g_pan_moves != 0 || g_switch_refused != 0) {
    ModLog("god view     : the mouse took the land %llu times and moved the "
           "view on %llu frames. %llu frames waited for a ray that meets the "
           "land. The camera switches refused %llu moves.", g_pans,
           g_pan_moves, g_pan_held, g_switch_refused);
  }
  if (g_spawn_moves != 0 || g_spawn_same != 0) {
    ModLog("god view     : the view went to a new place of the game camera "
           "%llu times, and %llu times it was there already (god_spawn=%d)",
           g_spawn_moves, g_spawn_same, g_settings.god_spawn);
  }
  if (g_holds != 0) {
    ModLog("god view     : the smooth zoom ran %llu times, for %.2f s in all "
           "(god_zoom_smooth=%d)", g_holds, g_hold_seconds,
           g_settings.god_zoom_smooth);
  }
  if (g_walks != 0 || g_walk_moves != 0) {
    ModLog("god view     : the walk with the stick ran %llu times, for %.2f s "
           "in all, and it moved the view on %llu frames (move_stick=%d)",
           g_walks, g_walk_seconds, g_walk_moves, g_settings.move_stick);
  }
  ModLog("god view     : god_display_time=%d. %llu frames took the time of the "
         "walk and the smooth zoom from the display times of the headset",
         g_settings.god_display_time, g_display_frames);
  g_display_frames = 0;
  g_walks = g_walk_moves = 0;
  g_walk_seconds = 0.0;
  g_holds = 0;
  g_hold_seconds = 0.0;
  g_spawn_moves = g_spawn_same = 0;
  g_pans = g_pan_moves = g_pan_held = g_switch_refused = 0;
  g_god_steps = g_god_refused = 0;
}

}  // namespace

void TabletopReport() {
  if (!g_installed) return;
  CullDistanceReport();
  if (g_settings.god) {
    GodReport();
    CameraZoneReport();
    g_moves = 0;
    g_live_frames = g_frames = g_bracket_ends = 0;
    g_most_objects = g_most_sorted = 0;
    g_objects_full = g_sorted_full = 0;
    if (g_settings.sweep) TabletopToggle();
    return;
  }
  ModLog("tabletop     : the table owned %llu of %llu frames, and gave the "
         "game its camera back %llu times", g_live_frames, g_frames,
         g_bracket_ends);
  if (g_live_frames > 0) {
    ModLog("tabletop     : %llu land blocks cut and %llu kept, %llu sea discs, "
           "%llu cloud domes hidden, %llu key color clears of %llu rects "
           "each", g_blocks_cut, g_blocks_kept, g_sea_discs, g_mist_hidden,
           g_key_clears, g_key_clears > 0 ? g_key_rect_total / g_key_clears
                                          : 0ull);
  }
  ModLog("tabletop     : at most %d of %d map objects and %d of %d sorted "
         "objects in one pass. Full in %llu and %llu passes.",
         g_most_objects, kObjectLimit, g_most_sorted, kSortedLimit,
         g_objects_full, g_sorted_full);
  if (g_reach.have) {
    ModLog("tabletop     : the Hand reaches %.0f units, the game set %.0f",
           FloatOf(g_reach.written), FloatOf(g_reach.original));
  }
  if (g_have_table) {
    ModLog("tabletop     : the table center is at %.1f %.1f, heading %.2f "
           "degrees, %.1f units per meter, edge %.0f units. The grab moved it "
           "on %llu frames.", g_center[0], g_center[2], g_heading * 57.29578f,
           g_scale, g_radius, g_moves);
  }
  g_moves = 0;
  g_live_frames = g_frames = g_bracket_ends = 0;
  g_blocks_cut = g_blocks_kept = g_sea_discs = g_mist_hidden = 0;
  g_key_clears = g_key_rect_total = 0;
  g_most_objects = g_most_sorted = 0;
  g_objects_full = g_sorted_full = 0;
  // The test step of table_sweep. Each report window has one state, so the
  // profile lines of a window belong to it. The step is a cut, not a motion.
  if (g_settings.sweep) TabletopToggle();
}

bool TabletopSweeping() { return g_installed && g_settings.sweep; }

// THE GOD VIEW, for controllers.cpp and comfort_camera.cpp.
bool TabletopGodOwnsView() { return g_settings.god && TabletopOwnsView(); }

bool TabletopIsOn() { return g_installed && g_on; }

bool TabletopInstalled() { return g_installed; }

// The teleports that the view made since the last call, for the report.
uint64_t TabletopTakeTeleportMoves() {
  const uint64_t n = g_tele_moves;
  g_tele_moves = 0;
  return n;
}

// THE TELEPORT REQUESTS. teleport.cpp has checked the target. The move is
// made once, at the next main camera call, as one cut: the God view puts the
// head over the land point (GodTeleport), and the table puts the land point
// at the table center. A second request before then replaces the first.
bool TabletopGodTeleport(const float target[3]) {
  if (!TabletopGodOwnsView()) return false;
  g_tele.pending = true;
  g_tele.target[0] = target[0];
  g_tele.target[1] = 0.0f;
  g_tele.target[2] = target[2];
  return true;
}

bool TabletopTableTeleport(const float target[3]) {
  if (!TabletopOwnsView() || g_settings.god) return false;
  g_tele.pending = true;
  g_tele.target[0] = target[0];
  g_tele.target[1] = 0.0f;
  g_tele.target[2] = target[2];
  return true;
}

// THE GAME PUT ITS OWN CAMERA AT A NEW PLACE (docs/god-view.md, "The place at
// a camera of the game"). comfort_camera.cpp found a move of the game camera that the player did not
// make, and the camera is at rest. The view goes there at the next main
// camera call, once (ApplyGodSpawn). A second move before then replaces the
// first. With god_spawn=0 the view stays, and the log says so.
void TabletopGodGameMoved(const char* cause, const float position[3],
                          const float focus[3]) {
  if (!g_installed || !g_settings.god || !TabletopGodOwnsView()) return;
  if (g_settings.god_spawn == 0) {
    if (g_spawn_logs < kMaxSpawnLogs) {
      ++g_spawn_logs;
      ModLog("god view     : the game camera went to a new place (%s): it is "
             "at %.1f %.1f %.1f and looks at %.1f %.1f %.1f. The view stays "
             "where it is (god_spawn=0).", cause, position[0], position[1],
             position[2], focus[0], focus[1], focus[2]);
    }
    return;
  }
  g_spawn.pending = true;
  strncpy_s(g_spawn.cause, cause, _TRUNCATE);
  for (int i = 0; i < 3; ++i) {
    g_spawn.position[i] = position[i];
    g_spawn.focus[i] = focus[i];
  }
}

// The land point that a game camera looks at, for the watch of the game
// camera in comfort_camera.cpp (LookPoint).
void TabletopGodLookPoint(const float position[3], const float focus[3],
                          float out[3]) {
  LookPoint(position, focus, out);
}

// The game camera is at rest and the view stays: after a write of the mod
// (GodAfter in comfort_camera.cpp), and after the game brought its camera to
// rest again. The pair of that camera and the view of now is kept. When the
// game puts its camera there again, PlaceGod brings the view back exactly
// where it is now.
void TabletopGodCameraWritten(const float position[3], const float focus[3]) {
  if (!TabletopGodOwnsView()) return;
  g_pair.have = true;
  for (int i = 0; i < 3; ++i) {
    g_pair.position[i] = position[i];
    g_pair.focus[i] = focus[i];
  }
  g_pair.pose = Pose();
  g_pair.version = g_god_version;
}

// THE TURN THAT KEEPS THE GAME'S FOCUS (GodAfter in comfort_camera.cpp,
// god_turn_keeps_focus). True when the view only turned with snap turns since
// the pair of the game camera and the view was kept, and turned then gives by
// how much, in radians, to the right above 0. The game camera can then turn
// about its own focus, as the game's turn does, also when a script put that
// focus there.
bool TabletopGodTurnedOnly(float* turned) {
  if (!TabletopGodOwnsView() || !g_pair.have) return false;
  if (g_god_nonturn_version > g_pair.version) return false;
  *turned = Wrap(g_heading - g_pair.pose.heading);
  return true;
}

// THE TEST KEY of god_spawn (Ctrl and Numpad 0, with test_keys=1). The rule
// changes between 1 and 0. Then the view goes to the game camera of now by
// the rule that it has, so the two places can be compared at one camera.
void TabletopGodSpawnToggle() {
  if (!g_installed || !g_settings.god || !TestSwitchesOn()) return;
  g_settings.god_spawn = g_settings.god_spawn != 0 ? 0 : 1;
  ModLog("test key     : god_spawn=%d. %s", g_settings.god_spawn,
         g_settings.god_spawn != 0
             ? "The view follows the game camera, and the land point that it "
               "looks at is in front of your head"
             : "The view does not follow the game camera, and the land point "
               "that it looks at is under the recenter pose");
  if (TabletopGodOwnsView()) g_spawn.again = true;
}

// The value of god_spawn for the debug page.
int TabletopGodSpawnValue() {
  if (!g_installed || !g_settings.god || !TestSwitchesOn()) {
    return kDebugCannot;
  }
  return g_settings.god_spawn != 0 ? 1 : 0;
}

// THE TEST KEY of god_camera_at (Ctrl and Numpad 1, with test_keys=1). The
// version changes, so the game camera goes to the other place at its next
// update.
void TabletopGodCameraAtToggle() {
  if (!g_installed || !g_settings.god || !TestSwitchesOn()) return;
  g_settings.god_camera_at = g_settings.god_camera_at != 0 ? 0 : 1;
  ModLog("test key     : god_camera_at=%d. The game camera stands over %s",
         g_settings.god_camera_at,
         g_settings.god_camera_at != 0 ? "the land point under your head"
                                       : "the recenter place");
  ++g_god_version;
  g_god_nonturn_version = g_god_version;
}

// The value of god_camera_at for the debug page.
int TabletopGodCameraAtValue() {
  if (!g_installed || !g_settings.god || !TestSwitchesOn()) {
    return kDebugCannot;
  }
  return g_settings.god_camera_at != 0 ? 1 : 0;
}

// A step from the sticks or the wheel. It is kept until the next main camera
// call, which uses it once. A second push before then replaces it, so one
// frame never takes two steps of one kind.
//
// A zoom keeps to the game's switch for the zoom (bit 4), as the wheel of the
// game does. The tutorial turns it on at its own step. A refused zoom keeps
// nothing, and the caller gets false, so the controller gives no pulse.
bool TabletopGodStep(int zoom, int ground) {
  if (!TabletopGodOwnsView()) return false;
  if (zoom != 0 && (CameraFeatures() & kFeatureZoom) == 0) {
    ++g_god_refused;
    ++g_switch_refused;
    if (GodLog()) {
      ModLog("god view     : the game does not allow a zoom now (camera "
             "switch 4 is off), so the view does not zoom");
    }
    zoom = 0;
  }
  if (zoom != 0) g_god_zoom = zoom > 0 ? 1 : -1;
  if (ground != 0) g_god_ground = ground > 0 ? 1 : -1;
  return zoom != 0 || ground != 0;
}

// THE SMOOTH ZOOM: the request of the stick for the next frame
// (ApplyGodZoomHold). The last request before a main camera call counts, and
// that call uses it once.
//
// The smooth zoom keeps to the game's switch for the zoom (bit 4), as a zoom
// step does. A refused request keeps nothing, and the caller gets false. The
// stick then asks again only after it came back, so each refused push is
// counted and logged once.
bool TabletopGodZoomSmooth() {
  return g_installed && g_settings.god && g_settings.god_zoom_smooth != 0;
}

bool TabletopGodZoomForwardIn() {
  return g_settings.god_zoom_forward_in != 0;
}

bool TabletopGodZoomHold(float push) {
  if (!TabletopGodZoomSmooth() || !TabletopGodOwnsView()) return false;
  if (!(push == push) || push == 0.0f) return false;
  if ((CameraFeatures() & kFeatureZoom) == 0) {
    ++g_god_refused;
    ++g_switch_refused;
    if (GodLog()) {
      ModLog("god view     : the game does not allow a zoom now (camera "
             "switch 4 is off), so the view does not zoom");
    }
    StopZoomHold("the game turned its zoom switch off", true);
    return false;
  }
  if (push > 1.0f) push = 1.0f;
  if (push < -1.0f) push = -1.0f;
  g_hold.push = push;
  g_hold.fresh = true;
  return true;
}

// THE TEST KEY of god_zoom_smooth (Ctrl, Shift and Insert, with
// test_keys=1). The zoom of the stick changes between the steps and the
// smooth zoom. The pointing controller gives a long pulse for the smooth zoom
// and a short pulse for the steps. A hold that runs stops at once, and a
// stick that is pushed must come back before it zooms.
void TabletopGodZoomSmoothToggle() {
  if (!g_installed || !g_settings.god || !TestSwitchesOn()) return;
  g_settings.god_zoom_smooth = g_settings.god_zoom_smooth != 0 ? 0 : 1;
  if (g_settings.god_zoom_smooth == 0) {
    StopZoomHold("the test key chose the steps", true);
  }
  ControllersGesturePulse(0.5f,
                          g_settings.god_zoom_smooth != 0 ? 0.30f : 0.08f);
  ModLog("test key     : god_zoom_smooth=%d. %s", g_settings.god_zoom_smooth,
         g_settings.god_zoom_smooth != 0
             ? "The stick zooms while it is held, and the zoom stops when the "
               "stick comes back"
             : "One push of the stick is one zoom step");
}

// The value of god_zoom_smooth for the debug page.
int TabletopGodZoomSmoothValue() {
  if (!g_installed || !g_settings.god || !TestSwitchesOn()) {
    return kDebugCannot;
  }
  return g_settings.god_zoom_smooth != 0 ? 1 : 0;
}

// THE TEST KEY of god_zoom_about (Ctrl, Shift and Delete, with test_keys=1).
// The point of a zoom goes from the land that the head looks at (1) to the
// land under the head (0), to the Hand's point (2) and back. The pointing
// controller gives a short pulse for 0, a medium pulse for 1 and a long pulse
// for 2. A hold of the smooth zoom that runs stops, and a stick that is still
// pushed starts a new hold with the new point.
void TabletopGodZoomAboutCycle() {
  if (!g_installed || !g_settings.god || !TestSwitchesOn()) return;
  g_settings.god_zoom_about = zoom_math::NextAbout(g_settings.god_zoom_about);
  StopZoomHold("the test key changed the point of the zoom", true);
  const int about = g_settings.god_zoom_about;
  ControllersGesturePulse(0.5f, about == zoom_math::kAboutHand   ? 0.40f
                                : about == zoom_math::kAboutLook ? 0.24f
                                                                 : 0.08f);
  LogZoomAbout("test key     ");
}

// The value of god_zoom_about for the debug page: 0, 1 or 2.
int TabletopGodZoomAboutValue() {
  if (!g_installed || !g_settings.god || !TestSwitchesOn()) {
    return kDebugCannot;
  }
  return g_settings.god_zoom_about;
}

// THE WALK WITH THE STICK: the rule of now, and the request of the stick for
// the next frame (ApplyGodWalk). The last request before a main camera call
// counts, and that call uses it once.
//
// The walk keeps to the game's switch for the drag of the land (bit 8), as a
// grab of the world does. A refused request keeps nothing, and the caller
// gets false. The stick then asks again only after it came back, so each
// refused push is counted and logged once.
int TabletopGodMoveStick() {
  if (!g_installed || !g_settings.god) return walk_math::kRuleOff;
  return walk_math::Rule(g_settings.move_stick);
}

bool TabletopMoveAllowed() { return (CameraFeatures() & kFeatureDrag) != 0; }

bool TabletopGodWalk(const float push[2], const float* aim, const float* aim3) {
  if (TabletopGodMoveStick() == walk_math::kRuleOff || !TabletopGodOwnsView()) {
    return false;
  }
  if (!(push[0] == push[0]) || !(push[1] == push[1])) return false;
  if (push[0] == 0.0f && push[1] == 0.0f) return false;
  if ((CameraFeatures() & kFeatureDrag) == 0) {
    ++g_god_refused;
    ++g_switch_refused;
    if (GodLog()) {
      ModLog("god view     : the game does not allow a drag of the land now "
             "(camera switch 8 is off), so the stick does not move you");
    }
    StopWalk("the game turned its switch for the drag of the land off", true);
    return false;
  }
  g_walk.push[0] = push[0];
  g_walk.push[1] = push[1];
  g_walk.have_aim = aim != nullptr;
  if (aim != nullptr) {
    g_walk.aim[0] = aim[0];
    g_walk.aim[1] = aim[1];
  }
  g_walk.have_aim3 = aim3 != nullptr;
  if (aim3 != nullptr) {
    for (int i = 0; i < 3; ++i) g_walk.aim3[i] = aim3[i];
  }
  g_walk.fresh = true;
  return true;
}

// THE TEST KEY of move_stick (Shift and the Menu key, with test_keys=1). The
// rule goes from the head (1) to the controller (2), to off (0) and back.
// The pointing controller gives a short pulse for 0, a medium pulse for 1
// and a long pulse for 2. A walk that runs stops, and a stick that is still
// pushed starts a new walk with the new rule.
void TabletopGodMoveStickCycle() {
  if (!g_installed || !g_settings.god || !TestSwitchesOn()) return;
  g_settings.move_stick =
      walk_math::NextRule(walk_math::Rule(g_settings.move_stick));
  StopWalk("the test key changed move_stick", true);
  const int rule = g_settings.move_stick;
  ControllersGesturePulse(0.5f, rule == walk_math::kRuleController ? 0.40f
                                : rule == walk_math::kRuleHead     ? 0.24f
                                                                   : 0.08f);
  LogMoveStick("test key     ");
}

// The settings window (settings_panel.cpp): move_stick,
// move_stick_speed_cm and god_zoom_smooth at once. A walk or a hold of the
// zoom that runs stops, as for the test keys.
void TabletopGodSetMoveStick(int rule, const char* by) {
  g_settings.move_stick = walk_math::Rule(rule);
  if (g_installed) StopWalk("the settings window changed move_stick", true);
  ModLog("tabletop     : %s: move_stick=%d now", by, g_settings.move_stick);
}

void TabletopGodSetMoveSpeed(int cm, const char* by) {
  if (cm < 10) cm = 10;
  if (cm > 500) cm = 500;
  g_settings.move_stick_speed_cm = cm;
  ModLog("tabletop     : %s: move_stick_speed_cm=%d now: a full push moves "
         "you %d cm of the room each second", by, cm, cm);
}

void TabletopGodSetZoomSmooth(int smooth, const char* by) {
  g_settings.god_zoom_smooth = smooth != 0 ? 1 : 0;
  if (g_installed && smooth == 0) {
    StopZoomHold("the settings window chose the steps", true);
  }
  ModLog("tabletop     : %s: god_zoom_smooth=%d now. %s", by,
         g_settings.god_zoom_smooth,
         g_settings.god_zoom_smooth != 0
             ? "The stick zooms while it is held"
             : "One push of the stick is one zoom step");
}

// THE KIND OF MOVE (debug page, switch "Move kind"): the walk that follows
// the land, the walk that keeps its height, and the fly. The pointing
// controller gives a short pulse for the walk, a medium pulse for the level
// walk and a long pulse for the fly. A walk or a fly that runs stops, and a
// stick that is still pushed starts a new one.
void TabletopGodMoveKindCycle() {
  if (!g_installed || !g_settings.god || !TestSwitchesOn()) return;
  const int kind = walk_math::NextKind(walk_math::Kind(
      g_settings.move_type, g_settings.move_stick_follow_land));
  g_settings.move_type = kind == walk_math::kKindFly ? walk_math::kTypeFly
                                                     : walk_math::kTypeWalk;
  if (kind != walk_math::kKindFly) {
    g_settings.move_stick_follow_land = kind == walk_math::kKindWalk ? 1 : 0;
  }
  StopWalk("the debug page changed the kind of move", true);
  ControllersGesturePulse(0.5f, kind == walk_math::kKindFly         ? 0.40f
                                : kind == walk_math::kKindWalkLevel ? 0.24f
                                                                    : 0.08f);
  LogMoveStick("debug page   ");
}

int TabletopGodMoveKindValue() {
  if (!g_installed || !g_settings.god || !TestSwitchesOn()) {
    return kDebugCannot;
  }
  return walk_math::Kind(g_settings.move_type,
                         g_settings.move_stick_follow_land);
}

// THE SWITCH "Speed ramp" of the debug page: move_stick_ramp off and on. A
// walk or a fly that runs stops, and a stick that is still pushed starts
// again at the speed of the push.
void TabletopGodMoveRampToggle() {
  if (!g_installed || !g_settings.god || !TestSwitchesOn()) return;
  g_settings.move_stick_ramp = g_settings.move_stick_ramp != 0 ? 0 : 1;
  StopWalk("the debug page changed move_stick_ramp", true);
  ControllersGesturePulse(0.5f, g_settings.move_stick_ramp != 0 ? 0.40f
                                                                : 0.08f);
  ModLog("debug page   : %s", RampText());
}

// THE SWITCH "Fly at solids" of the debug page: fly_solid_slide, the slide
// past a solid (1) or the stop short of it (0).
void TabletopGodFlySlideToggle() {
  if (!g_installed || !g_settings.god || !TestSwitchesOn()) return;
  g_settings.fly_solid_slide = g_settings.fly_solid_slide != 0 ? 0 : 1;
  ControllersGesturePulse(0.5f, g_settings.fly_solid_slide != 0 ? 0.40f
                                                                : 0.08f);
  ModLog("debug page   : fly_solid_slide=%d. %s", g_settings.fly_solid_slide,
         g_settings.fly_solid_slide != 0
             ? "A fly slides past a house, a rock or a tree trunk"
             : "A fly stops short of a house, a rock or a tree trunk");
}

// THE SWITCH "Deep zoom" of the debug page: god_zoom_far_level, the level of
// a zoom in over a land more than 2.5 m below the head. A hold of the smooth
// zoom that runs stops, so the next hold takes its point by the new rule.
void TabletopGodZoomFarToggle() {
  if (!g_installed || !g_settings.god || !TestSwitchesOn()) return;
  g_settings.god_zoom_far_level = g_settings.god_zoom_far_level != 0 ? 0 : 1;
  StopZoomHold("the debug page changed god_zoom_far_level", true);
  ControllersGesturePulse(0.5f, g_settings.god_zoom_far_level != 0 ? 0.40f
                                                                   : 0.08f);
  ModLog("debug page   : god_zoom_far_level=%d. %s",
         g_settings.god_zoom_far_level,
         g_settings.god_zoom_far_level != 0
             ? "A zoom in over a land more than 2.5 m below your head keeps "
               "the land at its height in the room"
             : "A zoom in over a land more than 2.5 m below your head zooms "
               "about a level 2.5 m below your head, and the land goes "
               "farther down");
}

int TabletopGodZoomFarValue() {
  if (!g_installed || !g_settings.god || !TestSwitchesOn()) {
    return kDebugCannot;
  }
  return g_settings.god_zoom_far_level != 0 ? 1 : 0;
}

// THE SWITCH "Speed by scale" of the debug page: move_stick_scale off and
// on. A walk or a fly that runs stops, and a stick that is still pushed
// starts again with the new speed.
void TabletopGodMoveScaleToggle() {
  if (!g_installed || !g_settings.god || !TestSwitchesOn()) return;
  g_settings.move_stick_scale = g_settings.move_stick_scale != 0 ? 0 : 1;
  StopWalk("the debug page changed move_stick_scale", true);
  ControllersGesturePulse(0.5f, g_settings.move_stick_scale != 0 ? 0.40f
                                                                 : 0.08f);
  ModLog("debug page   : %s", MoveScaleText(g_scale));
}

int TabletopGodMoveScaleValue() {
  if (!g_installed || !g_settings.god || !TestSwitchesOn()) {
    return kDebugCannot;
  }
  return g_settings.move_stick_scale != 0 ? 1 : 0;
}

float TabletopGodMoveScaleFactor() {
  return g_installed && g_settings.god ? MoveScaleFactor(g_scale) : 1.0f;
}

// The curve of the speed by the scale at the scale now, with the numbers of
// move_stick_scale_* and with or without move_stick_scale, for the scale
// part of the gain of the grab (grab_gain_math.h).
float TabletopGodScaleCurve() {
  if (!g_installed || !g_settings.god) return 1.0f;
  return walk_math::ScaleFactor(
      g_scale, g_settings.move_stick_scale_ref_x10 * 0.1f,
      g_settings.move_stick_scale_power_pct * 0.01f,
      g_settings.move_stick_scale_low_pct * 0.01f,
      g_settings.move_stick_scale_high_pct * 0.01f);
}

uint64_t TabletopGodViewVersion() { return g_god_version; }

bool TabletopGodWalkActive() { return g_walk.active; }

int TabletopGodFlySlideValue() {
  if (!g_installed || !g_settings.god || !TestSwitchesOn()) {
    return kDebugCannot;
  }
  return g_settings.fly_solid_slide != 0 ? 1 : 0;
}

int TabletopGodMoveRampValue() {
  if (!g_installed || !g_settings.god || !TestSwitchesOn()) {
    return kDebugCannot;
  }
  return g_settings.move_stick_ramp != 0 ? 1 : 0;
}

// The settings window (settings_panel.cpp): move_type.
void TabletopGodSetMoveType(int type, const char* by) {
  g_settings.move_type = walk_math::Type(type);
  if (g_installed) StopWalk("the settings window changed move_type", true);
  ModLog("tabletop     : %s: move_type=%d now: the stick %s", by,
         g_settings.move_type,
         Flies() ? "flies along the forward direction in 3D"
                 : "walks over the land");
}

// The value of move_stick for the debug page: 0, 1 or 2.
int TabletopGodMoveStickValue() {
  if (!g_installed || !g_settings.god || !TestSwitchesOn()) {
    return kDebugCannot;
  }
  return walk_math::Rule(g_settings.move_stick);
}

bool TabletopGodTurn(int direction, float degrees) {
  if (!TabletopGodOwnsView() || direction == 0 || !(degrees > 0.0f)) {
    return false;
  }
  g_god_turn = direction > 0 ? 1 : -1;
  g_god_turn_degrees = degrees;
  return true;
}

// THE SMOOTH TURN: the request of comfort_camera.cpp for the next frame
// (ApplyGodTurnHold). The last request before a main camera call counts, and
// that call uses it once.
bool TabletopGodTurnHold(float share, float degrees_per_second) {
  if (!TabletopGodOwnsView() || !(share == share) || share == 0.0f ||
      !(degrees_per_second > 0.0f)) {
    return false;
  }
  if (share > 1.0f) share = 1.0f;
  if (share < -1.0f) share = -1.0f;
  g_turn_hold.share = share;
  g_turn_hold.rate = degrees_per_second;
  g_turn_hold.fresh = true;
  return true;
}

// The degrees that the smooth turn turned since the last call, for the turn
// events of the tutorial, and their direction (1 right, -1 left).
float TabletopGodTakeSmoothTurn(int* direction) {
  const float degrees = g_turn_hold.help;
  *direction = g_turn_hold.help_direction;
  g_turn_hold.help = 0.0f;
  return degrees;
}

// THE GAME'S CAMERA OVER THE PLAYER'S PLACE. It looks at the land under the
// recenter pose, along the heading of the view, from 400 units away and 50
// degrees above. So the game decides the sounds, the detail and its limits
// from a camera near the place the player sees, at a distance the game was
// made for. The land point stays within 3000 units of the map center.
//
// IT MOVES ONLY WHEN THE PLAYER MOVES THE VIEW, once for each move (version).
// It does not follow the head: GCamera::Update sets its moving flag for a
// camera that changes, and the interface then clears the gesture trail
// (docs/backend.md, "The story camera holds its shot"). And it does not
// undo the game's own moves of its camera, for example a script that moves
// it and waits for it to arrive. At the build the game camera is already
// there, because the view is built from it.
bool TabletopGodGameCamera(float position[3], float focus[3],
                           uint64_t* version) {
  if (!TabletopGodOwnsView()) return false;
  *version = g_god_version;
  // The place: the land point under the head, or the recenter place. It is
  // latched for each move of the view, so a head that moves with a still
  // view does not move the game camera.
  static uint64_t placed = ~0ull;
  static int placed_at = -1;
  static float place[2] = {0.0f, 0.0f};
  if (placed != g_god_version || placed_at != g_settings.god_camera_at) {
    placed = g_god_version;
    placed_at = g_settings.god_camera_at;
    place[0] = g_center[0];
    place[1] = g_center[2];
    if (g_settings.god_camera_at != 0) {
      float room[3], head[3], under[3];
      TableRoom(room);
      HeadRoom(head);
      PoseWorld(Pose(), room, head, under);
      place[0] = under[0];
      place[1] = under[2];
    }
    // In a camera zone of a script the land point goes into the zone, and
    // GodAfter in comfort_camera.cpp keeps the position in it.
    CameraZoneFocus(&place[0], &place[1]);
    // The start of a hold of the smooth zoom has its own line, with the
    // place and the scale, and so have the start and the end of a walk with
    // the stick.
    static int said = 0;
    if (said < 40 && g_scale > 0.0f && !g_hold.active && !g_walk.active &&
        !g_turn_hold.active) {
      ++said;
      ModLog("god view     : the game camera is over the %s at %.1f %.1f, %.1f "
             "units per meter",
             g_settings.god_camera_at != 0 ? "land under your head"
                                           : "recenter place",
             place[0], place[1], g_scale);
    }
  }
  float dx = place[0] - kMapCenter, dz = place[1] - kMapCenter;
  const float out = sqrtf(dx * dx + dz * dz);
  if (out > kGodGameRadius) {
    dx *= kGodGameRadius / out;
    dz *= kGodGameRadius / out;
  }
  focus[0] = kMapCenter + dx;
  focus[2] = kMapCenter + dz;
  focus[1] = GroundAt(focus[0], focus[2]);
  GameCameraOver(focus, g_heading, kGodGameDistance, kGodGamePitch, position);
  return true;
}
