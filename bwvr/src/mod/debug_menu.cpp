// The debug page of the ring. See debug_menu.h, docs/controllers.md, "The
// debug page", and docs/backend.md, "Keys for v1.0".
//
// WHY. The test keys of the mod (test_keys=1) change one rule each while the
// game runs, for a live A/B. A player in the headset has no keyboard. With
// debug_menu=1 the ring has the item "debug", and its pages hold the same
// switches: the player chooses one with the hand, and the switch changes at
// once.
//
// HOW. kSwitches below is the one table of the switches. A line has the
// group, the word on the badge, the name on the card, the ini key, the
// function that gives the value now, the function that changes it, and the
// words of the values. The change function is the function that the test
// key calls, so the page and the key make the same step, the same pulse and
// the same log line. A switch whose value is kDebugCannot is not on the page
// at that time. The ring (radial_menu.cpp) builds the pages
// (debug_menu_math.h) each frame that it is closed.
//
// THE KEYS of bwvr.ini:
//
//   debug_menu=0     1 puts the item "debug" on the ring. It does not need
//                    test_keys=1. The keys of the keyboard still need
//                    test_keys=1
//   debug_window=1   1: the item "debug" opens the debug window
//                    (debug_window.cpp). 0: it goes to the debug pages of
//                    the ring

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "debug_menu.h"
#include "debug_menu_math.h"

extern void ModLog(const char* format, ...);
uint64_t ControllersFrameCount();

// --- the switches, from the files of their areas ----------------------------------

// The ring (radial_menu.cpp).
int RadialMenuWayValue();
void RadialMenuWayChange(const char* by);
// The controllers (controllers.cpp).
int ControllersOwnerModeValue();
void ControllersCycleOwnerMode();
int ControllersButtonLayoutValue();
void ControllersButtonLayoutToggle(const char* by);
int ControllersPressGuardValue();
void ControllersPressGuardChange(const char* by);
int ControllersPressHoldValue();
void ControllersPressHoldChange(const char* by);
int ControllersMenuStopsTurnValue();
void ControllersMenuStopsTurnChange(const char* by);
int ControllersRecenterHoldValue();
void ControllersRecenterHoldChange(const char* by);
// The comfort vignette (vignette.cpp).
int VignetteValue();
void VignetteChange(const char* by);
// The overlay of a head under the land (head_blinder.cpp).
int HeadBlinderValue();
void HeadBlinderChange(const char* by);
int HeadBlinderObjectsValue();
void HeadBlinderObjectsChange(const char* by);
// The God view and the table (tabletop.cpp), the grab of the world
// (world_grab.cpp).
int TabletopValue();
void TabletopChange(const char* by);
int TabletopGodSpawnValue();
void TabletopGodSpawnToggle();
int TabletopGodCameraAtValue();
void TabletopGodCameraAtToggle();
int TabletopGodZoomSmoothValue();
void TabletopGodZoomSmoothToggle();
int TabletopGodZoomAboutValue();
void TabletopGodZoomAboutCycle();
int TabletopGodZoomFarValue();
void TabletopGodZoomFarToggle();
int TabletopGodMoveStickValue();
void TabletopGodMoveStickCycle();
int TabletopGodMoveKindValue();
void TabletopGodMoveKindCycle();
int TabletopGodMoveRampValue();
void TabletopGodMoveRampToggle();
int TabletopGodFlySlideValue();
void TabletopGodFlySlideToggle();
int WorldGrabTestValue();
void WorldGrabCycleTest();
// The teleport (teleport.cpp).
int TeleportRingValue();
void TeleportRingChange(const char* by);
int TeleportSwitchValue();
void TeleportSwitchToggle(const char* by);
// The camera zone of a script of the game (camera_zone.cpp).
int CameraZoneValue();
void CameraZoneToggle();
// The interface (ui_panel.cpp, draw_hooks.cpp, bubble_card.cpp,
// note_depth.cpp, hint_card.cpp, text_click.cpp, spirit_anchor.cpp).
int UiPanelValue();
void UiPanelToggle();
int UiPanelSubtitleReadAheadValue();
void UiPanelSubtitleReadAheadToggle(const char* by);
int UiDepthSubtitleDegreesValue();
void UiDepthCycleSubtitleDegrees();
int UiDepthHintHandDepthValue();
void UiDepthToggleHintHandDepth(const char* by);
int BubbleCardValue();
void BubbleCardToggle(const char* by);
int NoteDepthValue();
void NoteDepthToggle(const char* by);
int HintCardTimingValue();
void HintCardTimingChange(const char* by);
int HintCardWordsValue();
void HintCardWordsChange(const char* by);
int HintCardMovesValue();
void HintCardMovesToggle(const char* by);
int HintCardStyleValue();
void HintCardStyleToggle(const char* by);
int HintCardPicturesValue();
void HintCardPicturesToggle(const char* by);
int TextClickValue();
void TextClickChange(const char* by);
int SpiritAnchorWorldSizeValue();
void SpiritAnchorWorldSizeToggle();
// The picture (cutscene_window.cpp, eye_clean.cpp, one_walk.cpp,
// land_light.cpp, vk_scene.cpp).
int CutsceneWindowValue();
void CutsceneWindowToggle();
int CutsceneWindowDepthValue();
void CutsceneWindowDepthToggle();
int EyeCleanValue();
void EyeCleanToggle(const char* by);
int OneWalkMarksValue();
void OneWalkMarksToggle(const char* by);
int LandLightEyesValue();
void LandLightEyesChange(const char* by);
int SeaDepthValue();
void SeaDepthChange(const char* by);
int CullDistanceValue();
void CullDistanceChange(const char* by);
int CullFogValue();
void CullFogChange(const char* by);
int InfluenceEdgeLookValue();
void InfluenceEdgeLookChange(const char* by);
int VkSceneEyesValue();
void VkSceneEyesToggle(const char* by);
int VkSceneKeepPictureValue();
void VkSceneKeepPictureToggle(const char* by);
int VkSceneStage1Value();
void VkSceneStage1Toggle(const char* by);
int VkSceneUntexturedValue();
void VkSceneUntexturedToggle(const char* by);
// The render (one_walk.cpp, frame_time.cpp, frame_send.cpp, render_thread.cpp,
// fast_transform.cpp, d3d7_thread.cpp, thread_wait.cpp, vk_scene.cpp,
// vk_window.cpp, mod_main.cpp, file_read_ahead.cpp).
int OneWalkValue();
void OneWalkToggle();
int OneWalkMergeValue();
void OneWalkMergeToggle(const char* by);
int FrameTimeValue();
void FrameTimeToggle(const char* by);
int FrameSendMode();
void FrameSendCycle(const char* by);
int RenderThreadValue();
void RenderThreadToggle(const char* by);
int RenderThreadLockFreeValue();
void RenderThreadLockFreeToggle(const char* by);
int RenderThreadFilterValue();
void RenderThreadFilterToggle(const char* by);
int RenderThreadBuffersValue();
void RenderThreadBuffersToggle(const char* by);
int RenderThreadFlushValue();
void RenderThreadFlushToggle(const char* by);
int RenderThreadRightEyeValue();
void RenderThreadRightEyeToggle(const char* by);
int RenderThreadLateEndValue();
void RenderThreadLateEndToggle(const char* by);
int FastTransformValue();
void FastTransformToggle(const char* by);
int D3D7ThreadValue();
void D3D7ThreadToggle(const char* by);
int D3D7ThreadMergeValue();
void D3D7ThreadMergeToggle(const char* by);
int ThreadWaitMode();
void ThreadWaitToggle(const char* by);
int VkSceneFollowValue();
void VkSceneFollowToggle(const char* by);
int VkScenePanelValue();
void VkScenePanelToggle(const char* by);
int VkSceneOnlyValue();
void VkSceneOnlyToggle(const char* by);
int VkSceneUploadsValue();
void VkSceneUploadsToggle(const char* by);
int VkSceneFoveationValue();
void VkSceneFoveationToggle(const char* by);
int VkSceneFoveaResValue();
void VkSceneFoveaResToggle(const char* by);
int VkWindowValue();
void VkWindowToggle(const char* by);
int XrVulkanUploadValue();
void XrVulkanUploadToggle(const char* by);
int XrHoldValue();
void XrHoldToggle(const char* by);
int FileReadAheadValue();
void FileReadAheadToggle(const char* by);
// The Hand (hand_direct.cpp, hand_exact.cpp, hand_feel.cpp, hand_model.cpp,
// target_glow.cpp, take_stream.cpp, forest_take.cpp).
int HandDirectReleaseAtOnceValue();
void HandDirectReleaseAtOnceChange(const char* by);
int HandDirectCatchMovingValue();
void HandDirectCatchMovingChange(const char* by);
int HandDirectWidePickValue();
void HandDirectWidePickChange(const char* by);
int HandDirectFarChoiceValue();
void HandDirectFarChoiceChange(const char* by);
int HandDirectFarStayValue();
void HandDirectFarStayChange(const char* by);
int HandDirectTakeCarryValue();
void HandDirectTakeCarryChange(const char* by);
int HandDirectNearRuleValue();
void HandDirectNearRuleChange(const char* by);
int HandDirectScrollDrawValue();
void HandDirectScrollDrawChange(const char* by);
int HandDirectScriptPlayerValue();
void HandDirectScriptPlayerChange(const char* by);
int HandDirectPalmPointValue();
void HandDirectPalmPointChange(const char* by);
int HandDirectStrokeLimitsValue();
void HandDirectStrokeLimitsChange(const char* by);
int HandDirectSlapValue();
void HandDirectSlapChange(const char* by);
int HandDirectStrokeHandValue();
void HandDirectStrokeHandChange(const char* by);
int HandDirectGlowIdleValue();
void HandDirectGlowIdleChange(const char* by);
int HandDirectRootsValue();
void HandDirectRootsToggle(const char* by);
int HandDirectTreeHintValue();
void HandDirectTreeHintToggle(const char* by);
int HandExactHideValue();
void HandExactHideChange(const char* by);
int HandExactHideAllValue();
void HandExactHideAllChange(const char* by);
int HandExactFlowValue();
void HandExactFlowChange(const char* by);
int HandFeelValue();
void HandFeelChange(const char* by);
int HandFeelPinchValue();
void HandFeelPinchChange(const char* by);
int HandLandStopValue();
void HandLandStopChange(const char* by);
int HandObjectStopValue();
void HandObjectStopChange(const char* by);
int ControllersNearTakeKeepsValue();
void ControllersNearTakeKeepsChange(const char* by);
int HandModelSwapValue();
void HandModelSwapChange(const char* by);
int HandModelLookValue();
void HandModelLookChange(const char* by);
// The frame of the controller on the grip pose (grip_fit.cpp).
int GripFitValue();
void GripFitChange(const char* by);
int TargetGlowValue();
void TargetGlowChange(const char* by);
int TakeStreamValue();
void TakeStreamChange(const char* by);
int ForestTakeValue();
void ForestTakeChange(const char* by);
// The throw (throw_physics.cpp).
int ThrowLawValue();
void ThrowLawChange(const char* by);
int ThrowDrawValue();
void ThrowDrawChange(const char* by);
int ThrowFallValue();
void ThrowFallChange(const char* by);
int ThrowLandValue();
void ThrowLandChange(const char* by);
int ThrowSettleValue();
void ThrowSettleChange(const char* by);
int ThrowTurnValue();
void ThrowTurnChange(const char* by);
int ThrowRestValue();
void ThrowRestChange(const char* by);
int ThrowTakeValue();
void ThrowTakeChange(const char* by);
int ThrowCatchValue();
void ThrowCatchChange(const char* by);
int ThrowSlowValue();
void ThrowSlowerChange(const char* by);
void ThrowFasterChange(const char* by);
int ThrowLandRateValue();
void ThrowLandSlowerChange(const char* by);
void ThrowLandFasterChange(const char* by);
// The Creature, the villagers and the miracles (creature_command.cpp,
// creature_stay.cpp, miracle.cpp, village.cpp, spell_book.cpp).
int CreatureCommandRuleValue();
void CreatureCommandRuleChange(const char* by);
int CreatureCommandLookValue();
void CreatureCommandLookChange(const char* by);
int CreatureStayModeValue();
void CreatureStayModeChange(const char* by);
// The Creature with no press (controllers.cpp, ui_panel.cpp).
int ControllersOpenStrokeValue();
void ControllersOpenStrokeChange(const char* by);
int UiPanelLingerValue();
void UiPanelLingerChange(const char* by);
int MiracleHandValue();
void MiracleHandChange(const char* by);
int VillageTotemValue();
void VillageTotemChange(const char* by);
int VillageHoldValue();
void VillageHoldChange(const char* by);
int VillageWristTurnValue();
void VillageWristTurnChange(const char* by);
// The spell book (spell_book.cpp).
int SpellBookValue();
void SpellBookChange(const char* by);
int SpellBookPullValue();
void SpellBookPullChange(const char* by);
int SpellBookBubbleValue();
void SpellBookBubbleChange(const char* by);
int SpellBookAllValue();
void SpellBookAllChange(const char* by);
// The draw of a gesture (controllers.cpp, overlay.cpp).
int ControllersGesturePointValue();
void ControllersGesturePointChange(const char* by);
int OverlayTrailValue();
void OverlayTrailChange(const char* by);
int OverlayRayGlowValue();
int OverlayRayGlowOnlyValue();
void OverlayRayGlowOnlyChange(const char* by);
// The glow at the end of the menu ray (overlay.cpp, ray_card.cpp).
int OverlayRayTipValue();
void OverlayRayTipChange(const char* by);
// The ray over the panels in the room (ray_card.cpp).
int RayCardValue();
void RayCardChange(const char* by);
// The view for videos (video_view.cpp).
int VideoViewValue();
void VideoViewChange(const char* by);
int VideoViewMarksValue();
void VideoViewMarksChange(const char* by);
int VideoViewLayersValue();
void VideoViewLayersChange(const char* by);
int VideoViewDownValue();
void VideoViewLowerChange(const char* by);
void VideoViewHigherChange(const char* by);
void OverlayRayGlowChange(const char* by);
// The sound (audio_listener.cpp, audio_voices.cpp, audio_range.cpp).
int AudioModeValue();
void AudioCycleMode();
int AudioStopWaitValue();
void AudioStopWaitToggle();
int AudioTakeLoopValue();
void AudioTakeLoopChange(const char* by);
// The Temple (temple.cpp).
int TemplePointerValue();
void TemplePointerChange(const char* by);
int TempleFlightValue();
void TempleFlightChange(const char* by);
int TempleViewValue();
void TempleViewChange(const char* by);
int TempleFadeValue();
void TempleFadeChange(const char* by);

namespace {

enum Group {
  kGroupView,
  kGroupPicture,
  kGroupRender,
  kGroupInterface,
  kGroupHand,
  kGroupThrow,
  kGroupCreature,
  kGroupSound,
  kGroupTemple,
  kGroups
};

const wchar_t* const kGroupName[kGroups] = {
    L"View",  L"Picture",  L"Render", L"Interface", L"Hand",
    L"Throw", L"Creature", L"Sound",  L"Temple"};

struct Switch {
  int group;
  const wchar_t* label;   // the badge, at most 14 letters
  const wchar_t* name;    // the card in the middle
  const char* key;        // the ini key, or "" for a state of a test only
  int (*value)();
  void (*change)(const char* by);
  const wchar_t* words;   // the words of the values from 0, with "|"
  // The debug window (debug_window.cpp). `back` is the step to the value
  // before, for the left arrow, or null: then the left arrow makes the
  // step of `change` too. A line with `in_window` false is on the ring
  // only, because a line with a `back` holds its step. `window_label` is
  // the label of that row, or null for `label`.
  void (*back)(const char* by) = nullptr;
  bool in_window = true;
  const wchar_t* window_label = nullptr;
};

// THE TABLE. One line for each test key that is a switch. A new test key is
// one more line here, with its two functions in the file of its area.
const Switch kSwitches[] = {
    // View
    {kGroupView, L"Table", L"Table or God view", "", TabletopValue,
     TabletopChange, L"off|on"},
    {kGroupView, L"Follow camera", L"God view follows the game camera",
     "god_spawn", TabletopGodSpawnValue,
     [](const char*) { TabletopGodSpawnToggle(); }, L"off|on"},
    {kGroupView, L"Camera place", L"Game camera over the head",
     "god_camera_at", TabletopGodCameraAtValue,
     [](const char*) { TabletopGodCameraAtToggle(); }, L"recenter|head"},
    {kGroupView, L"Smooth zoom", L"Stick zooms while it is held",
     "god_zoom_smooth", TabletopGodZoomSmoothValue,
     [](const char*) { TabletopGodZoomSmoothToggle(); }, L"steps|smooth"},
    {kGroupView, L"Zoom point", L"Point that a zoom keeps", "god_zoom_about",
     TabletopGodZoomAboutValue,
     [](const char*) { TabletopGodZoomAboutCycle(); },
     L"under head|look|Hand"},
    {kGroupView, L"Stick walk", L"Direction of the walk stick", "move_stick",
     TabletopGodMoveStickValue,
     [](const char*) { TabletopGodMoveStickCycle(); },
     L"off|head|controller"},
    {kGroupView, L"Move kind", L"Walk, level walk or fly", "",
     TabletopGodMoveKindValue,
     [](const char*) { TabletopGodMoveKindCycle(); },
     L"walk|walk level|fly"},
    {kGroupView, L"Speed ramp", L"Full push gets faster", "move_stick_ramp",
     TabletopGodMoveRampValue,
     [](const char*) { TabletopGodMoveRampToggle(); }, L"off|on"},
    {kGroupView, L"Fly at solids", L"Fly past a house or a tree",
     "fly_solid_slide", TabletopGodFlySlideValue,
     [](const char*) { TabletopGodFlySlideToggle(); }, L"stop|slide"},
    {kGroupView, L"Vignette", L"Edges dark in a move", "comfort_vignette",
     VignetteValue, VignetteChange, L"off|gentle|strong"},
    {kGroupView, L"Head blinder", L"Cover the view in the land",
     "head_blinder", HeadBlinderValue, HeadBlinderChange, L"off|on"},
    {kGroupView, L"Blinder solids", L"Cover the view in a house",
     "head_blinder_objects", HeadBlinderObjectsValue, HeadBlinderObjectsChange,
     L"land only|solids too"},
    {kGroupView, L"Menu turn", L"No turn in a dialog box", "menu_stops_turn",
     ControllersMenuStopsTurnValue, ControllersMenuStopsTurnChange,
     L"turns|stops"},
    {kGroupView, L"Recenter hold", L"Stick click held recenters",
     "recenter_hold", ControllersRecenterHoldValue,
     ControllersRecenterHoldChange, L"off|other hand|Hand"},
    {kGroupView, L"Grab test", L"Grab of the world: test step", "",
     WorldGrabTestValue, [](const char*) { WorldGrabCycleTest(); },
     L"as set|one to one|off"},
    {kGroupView, L"Ring size", L"Size rule of the teleport ring",
     "teleport_ring_mode", TeleportRingValue, TeleportRingChange,
     L"room|world"},
    {kGroupView, L"Jump switch", L"Game switch the teleport keeps to",
     "teleport_switch", TeleportSwitchValue, TeleportSwitchToggle,
     L"jump|drag"},
    {kGroupView, L"Camera zone", L"God view keeps to the camera zone",
     "god_zone", CameraZoneValue, [](const char*) { CameraZoneToggle(); },
     L"game|view"},
    {kGroupView, L"Button layout", L"Layout of the buttons", "button_layout",
     ControllersButtonLayoutValue, ControllersButtonLayoutToggle,
     L"ring on X|ring on Y"},
    {kGroupView, L"Ring ways", L"The ways that open the ring", "",
     RadialMenuWayValue, RadialMenuWayChange, L"both|button|stick"},
    {kGroupView, L"Pointer owner", L"Owner of the pointer", "",
     ControllersOwnerModeValue,
     [](const char*) { ControllersCycleOwnerMode(); },
     L"automatic|controller|mouse"},
    {kGroupView, L"Script player", L"Scripts see the player",
     "script_distance", HandDirectScriptPlayerValue,
     HandDirectScriptPlayerChange, L"camera|player"},
    {kGroupView, L"Deep zoom", L"Zoom in over far land", "god_zoom_far_level",
     TabletopGodZoomFarValue,
     [](const char*) { TabletopGodZoomFarToggle(); },
     L"2.5 m level|land level"},

    // Picture
    {kGroupPicture, L"Scene window", L"Cut scene on a screen in the room",
     "cutscene_window", CutsceneWindowValue,
     [](const char*) { CutsceneWindowToggle(); }, L"off|on"},
    {kGroupPicture, L"Scene depth", L"Depth limit of the cut scene",
     "cutscene_depth", CutsceneWindowDepthValue,
     [](const char*) { CutsceneWindowDepthToggle(); }, L"off|nearest|fixed"},
    {kGroupPicture, L"Eye clean", L"Own frame before for each eye",
     "eye_clean", EyeCleanValue, EyeCleanToggle, L"shared|own"},
    {kGroupPicture, L"Walk marks", L"Cards and marks for each eye",
     "one_walk_marks", OneWalkMarksValue, OneWalkMarksToggle, L"off|on"},
    {kGroupPicture, L"Land light", L"Lights of the land in both eyes",
     "land_light_eyes", LandLightEyesValue, LandLightEyesChange,
     L"left eye|both eyes"},
    {kGroupPicture, L"Sea depth", L"Land goes down under the water",
     "sea_depth", SeaDepthValue, SeaDepthChange, L"flat|depth"},
    {kGroupPicture, L"Cull distance", L"How far out models are drawn",
     "cull_distance", CullDistanceValue, CullDistanceChange,
     L"game|2x|3x|5x|10x"},
    {kGroupPicture, L"Cull fog", L"Fog moves out with the models",
     "cull_fog", CullFogValue, CullFogChange, L"game|follows"},
    {kGroupPicture, L"Edge look", L"Look of the influence edge",
     "influence_wall_look", InfluenceEdgeLookValue, InfluenceEdgeLookChange,
     L"patch|hologram"},
    {kGroupPicture, L"Advisor size", L"Advisor size at a world place",
     "spirit_world_size", SpiritAnchorWorldSizeValue,
     [](const char*) { SpiritAnchorWorldSizeToggle(); },
     L"game|near size"},
    {kGroupPicture, L"Vulkan eyes", L"Eye pictures from Vulkan", "vk_eyes",
     VkSceneEyesValue, VkSceneEyesToggle, L"D3D9|Vulkan"},
    {kGroupPicture, L"Keep picture", L"Walk starts from last picture",
     "vk_keep_picture", VkSceneKeepPictureValue, VkSceneKeepPictureToggle,
     L"black|keep"},
    {kGroupPicture, L"Stage 1", L"Second texture stage in Vulkan",
     "vk_stage1", VkSceneStage1Value, VkSceneStage1Toggle, L"off|on"},
    {kGroupPicture, L"No texture", L"Stage 0 with no texture",
     "vk_untextured", VkSceneUntexturedValue, VkSceneUntexturedToggle,
     L"white|diffuse"},
    {kGroupPicture, L"Ray glow", L"Glow along the rays and the arc",
     "ray_glow", OverlayRayGlowValue, OverlayRayGlowChange, L"off|on"},
    {kGroupPicture, L"Glow only", L"The glow is the whole ray",
     "ray_glow_only", OverlayRayGlowOnlyValue, OverlayRayGlowOnlyChange,
     L"tube|glow"},
    {kGroupPicture, L"Ray on panels", L"Ray over the panels in the room",
     "ray_over_panels", RayCardValue, RayCardChange, L"under|over"},
    {kGroupPicture, L"Ray tip", L"Round glow at the end of the ray",
     "ray_tip", OverlayRayTipValue, OverlayRayTipChange, L"mark|glow"},
    {kGroupPicture, L"Video view", L"16:9 view for videos in the window",
     "video_view", VideoViewValue, VideoViewChange, L"off|on"},
    {kGroupPicture, L"Video marks", L"Corner marks of the video area",
     "video_view_marks", VideoViewMarksValue, VideoViewMarksChange,
     L"off|on"},
    {kGroupPicture, L"Video layers", L"Cards and panels in the video",
     "video_view_layers", VideoViewLayersValue, VideoViewLayersChange,
     L"off|on"},
    {kGroupPicture, L"Video higher", L"Video area lower by, percent",
     "video_view_down_pct", VideoViewDownValue, VideoViewHigherChange, L"",
     nullptr, false},
    {kGroupPicture, L"Video lower", L"Video area lower by, percent",
     "video_view_down_pct", VideoViewDownValue, VideoViewLowerChange, L"",
     VideoViewHigherChange, true, L"Video height"},

    // Render
    {kGroupRender, L"One walk", L"One walk for both eyes", "one_walk",
     OneWalkValue, [](const char*) { OneWalkToggle(); }, L"off|on"},
    {kGroupRender, L"Foveation", L"Foveated rate of the eye pictures",
     "foveation", VkSceneFoveationValue, VkSceneFoveationToggle,
     L"off|on|marks"},
    {kGroupRender, L"Fovea images", L"Outer and centre eye images",
     "fovea_res", VkSceneFoveaResValue, VkSceneFoveaResToggle, L"off|on"},
    {kGroupRender, L"Frame send", L"Wait for the GPU in a frame",
     "frame_send", FrameSendMode, FrameSendCycle,
     L"each eye|one wait|2nd thread"},
    {kGroupRender, L"Frame hold", L"Hold of the frame, ms", "xr_hold_ms",
     XrHoldValue, XrHoldToggle, L""},
    {kGroupRender, L"Real time", L"Real frame time after the eyes",
     "frame_time_real", FrameTimeValue, FrameTimeToggle, L"kept|real"},
    {kGroupRender, L"SSE loops", L"Vertex loops with SSE", "fast_transform",
     FastTransformValue, FastTransformToggle, L"x87|SSE"},
    {kGroupRender, L"Thread wait", L"Threads spin, then sleep",
     "thread_wait", ThreadWaitMode, ThreadWaitToggle, L"spin|sleep"},
    {kGroupRender, L"Read ahead", L"Read ahead of the game files",
     "file_read_ahead", FileReadAheadValue, FileReadAheadToggle, L"off|on"},
    {kGroupRender, L"Follow target", L"Vulkan follows the render target",
     "vk_scene_follow", VkSceneFollowValue, VkSceneFollowToggle, L"off|on"},
    {kGroupRender, L"Upload queue", L"Texture uploads in the submit",
     "vk_upload_queue", VkSceneUploadsValue, VkSceneUploadsToggle,
     L"wait|queue"},
    {kGroupRender, L"Vulkan window", L"Window picture by a swapchain",
     "vk_window", VkWindowValue, VkWindowToggle, L"GDI|Vulkan"},
    {kGroupRender, L"Vulkan only", L"Walk with no Direct3D 9", "vk_only",
     VkSceneOnlyValue, VkSceneOnlyToggle, L"off|on"},
    {kGroupRender, L"Vulkan upload", L"CPU pixels to swapchains by Vulkan",
     "xr_vulkan_upload", XrVulkanUploadValue, XrVulkanUploadToggle,
     L"D3D11|Vulkan"},
    {kGroupRender, L"Render thread", L"Calls of a walk to render thread",
     "render_thread", RenderThreadValue, RenderThreadToggle, L"off|on|kept"},
    {kGroupRender, L"No lock", L"Texture answers with no lock",
     "render_thread_lock_free", RenderThreadLockFreeValue,
     RenderThreadLockFreeToggle, L"off|on"},
    {kGroupRender, L"Call filter", L"Drop calls that change nothing",
     "render_thread_filter", RenderThreadFilterValue,
     RenderThreadFilterToggle, L"off|on"},
    {kGroupRender, L"Own buffers", L"Render thread has own buffers",
     "render_thread_buffers", RenderThreadBuffersValue,
     RenderThreadBuffersToggle, L"off|on"},
    {kGroupRender, L"Merge eyes", L"One draw for both eyes",
     "one_walk_merge", OneWalkMergeValue, OneWalkMergeToggle, L"off|on"},
    {kGroupRender, L"GPU flush", L"Calls to the GPU each N draws",
     "gpu_flush", RenderThreadFlushValue, RenderThreadFlushToggle, L""},
    {kGroupRender, L"Right eye copy", L"Right eye copy on render thread",
     "render_thread_right_eye", RenderThreadRightEyeValue,
     RenderThreadRightEyeToggle, L"frame|render"},
    {kGroupRender, L"D3D7 thread", L"Direct3D 7 calls on own thread",
     "d3d7_thread", D3D7ThreadValue, D3D7ThreadToggle, L"off|on"},
    {kGroupRender, L"D3D7 merge", L"D3D7 thread merges draws",
     "d3d7_thread_merge", D3D7ThreadMergeValue, D3D7ThreadMergeToggle,
     L"off|on"},
    {kGroupRender, L"Late end", L"End of walk does not wait",
     "render_thread_late_end", RenderThreadLateEndValue,
     RenderThreadLateEndToggle, L"wait|no wait"},

    // Interface
    {kGroupInterface, L"UI layer", L"Interface on its own layer", "ui_panel",
     UiPanelValue, [](const char*) { UiPanelToggle(); },
     L"in picture|own layer|test"},
    {kGroupInterface, L"Subtitle angle", L"Subtitles below the view, degrees",
     "subtitle_deg", UiDepthSubtitleDegreesValue,
     [](const char*) { UiDepthCycleSubtitleDegrees(); }, L""},
    {kGroupInterface, L"Subtitle copy", L"Read the subtitle copy later",
     "subtitle_read_ahead", UiPanelSubtitleReadAheadValue,
     UiPanelSubtitleReadAheadToggle, L"wait|no wait"},
    {kGroupInterface, L"Bubble card", L"Speech bubble as one card",
     "bubble_card", BubbleCardValue, BubbleCardToggle, L"off|on"},
    {kGroupInterface, L"Note depth", L"Note of the game at its depth",
     "note_depth", NoteDepthValue, NoteDepthToggle, L"off|on"},
    {kGroupInterface, L"Hint depth", L"Hint at the Hand at Hand depth",
     "hint_hand_depth", UiDepthHintHandDepthValue,
     UiDepthToggleHintHandDepth, L"off|on"},
    {kGroupInterface, L"Vulkan panel", L"Panel of the Vulkan renderer",
     "vk_panel", VkScenePanelValue, VkScenePanelToggle, L"off|on"},
    {kGroupInterface, L"Prompt timing", L"A prompt shows at once",
     "prompt_timing", HintCardTimingValue, HintCardTimingChange, L"off|on"},
    {kGroupInterface, L"Hint words", L"Prompts and hints of the cards",
     "hint_words", HintCardWordsValue, HintCardWordsChange, L""},
    {kGroupInterface, L"Prompt moves", L"Lines show only when they move",
     "prompt_moves", HintCardMovesValue, HintCardMovesToggle, L"off|on"},
    {kGroupInterface, L"Card look", L"Cards, ring and keys as scrolls",
     "hint_card_style", HintCardStyleValue, HintCardStyleToggle,
     L"plain|scroll"},
    {kGroupInterface, L"Card pictures", L"Picture of the button on a card",
     "hint_card_pictures", HintCardPicturesValue, HintCardPicturesToggle,
     L"off|on"},
    {kGroupInterface, L"Tree card", L"Card of a tree in the Hand",
     "tree_plant_hint", HandDirectTreeHintValue, HandDirectTreeHintToggle,
     L"off|on"},
    {kGroupInterface, L"Text click", L"Trigger clicks a text of the game",
     "text_click", TextClickValue, TextClickChange, L"off|on"},

    // Hand
    {kGroupHand, L"Catch moving", L"Reach for an object that moves",
     "catch_moving", HandDirectCatchMovingValue, HandDirectCatchMovingChange,
     L"off|pick|rest|open"},
    {kGroupHand, L"Wide pick", L"Wide reach of the pick, in cm",
     "hand_wide_pick_cm", HandDirectWidePickValue, HandDirectWidePickChange,
     L""},
    {kGroupHand, L"Near rule", L"Rule of the pick at the Hand", "near_rule",
     HandDirectNearRuleValue, HandDirectNearRuleChange,
     L"old|nearest|one target"},
    {kGroupHand, L"Far choice", L"Choice of a far target", "far_grab_choice",
     HandDirectFarChoiceValue, HandDirectFarChoiceChange,
     L"point|bias|ranks|balls|ball ranks"},
    {kGroupHand, L"Far stay", L"Far target that stays", "far_grab_stay",
     HandDirectFarStayValue, HandDirectFarStayChange, L"off|on"},
    {kGroupHand, L"Take carry", L"Grip keeps a take with no source",
     "far_take_carry", HandDirectTakeCarryValue, HandDirectTakeCarryChange,
     L"off|on"},
    {kGroupHand, L"Roots pile", L"Roots pile of a far tree",
     "far_roots_at_tree", HandDirectRootsValue, HandDirectRootsToggle,
     L"glide|roots"},
    {kGroupHand, L"Take stream", L"Curve of a far take stream",
     "take_stream", TakeStreamValue, TakeStreamChange, L"line|curve"},
    {kGroupHand, L"Forest take", L"Bundle from a forest", "forest_take",
     ForestTakeValue, ForestTakeChange, L"off|on"},
    {kGroupHand, L"Target glow", L"Glow of a far target", "target_glow",
     TargetGlowValue, TargetGlowChange, L"halo|mesh|both"},
    {kGroupHand, L"Idle glow", L"Glow with no target", "hand_glow_idle",
     HandDirectGlowIdleValue, HandDirectGlowIdleChange, L"off|on"},
    {kGroupHand, L"Scroll draw", L"Each drawn scroll is a target",
     "scroll_draw", HandDirectScrollDrawValue, HandDirectScrollDrawChange,
     L"off|on"},
    {kGroupHand, L"Press guard", L"Guard of the first 300 ms",
     "grip_press_guard", ControllersPressGuardValue,
     ControllersPressGuardChange, L"off|on"},
    {kGroupHand, L"Press hold", L"Game button held for a pick",
     "grip_press_hold", ControllersPressHoldValue, ControllersPressHoldChange,
     L"off|on"},
    {kGroupHand, L"Take keeps", L"A take ends only on an open grip",
     "near_take_keeps", ControllersNearTakeKeepsValue,
     ControllersNearTakeKeepsChange, L"off|on"},
    {kGroupHand, L"Take guard", L"Hold back the rest during a take",
     "catch_rest_guard", ThrowTakeValue, ThrowTakeChange, L"off|on"},
    {kGroupHand, L"Catch at once", L"Catch a moving object at once",
     "catch_at_once", ThrowCatchValue, ThrowCatchChange, L"off|on"},
    {kGroupHand, L"Pinch", L"Pinch for a small object", "hand_pinch",
     HandFeelPinchValue, HandFeelPinchChange, L"fist|pinch"},
    {kGroupHand, L"Finger feel", L"Fingers react to the world", "hand_feel",
     HandFeelValue, HandFeelChange, L"off|on"},
    {kGroupHand, L"Land stop", L"Hand stops at the land", "hand_land_stop",
     HandLandStopValue, HandLandStopChange, L"off|on"},
    {kGroupHand, L"Object stop", L"Hand stops at a house or rock",
     "hand_object_stop", HandObjectStopValue, HandObjectStopChange,
     L"off|on"},
    {kGroupHand, L"Game Hand", L"Mesh of the game's Hand", "hand_hide_game",
     HandExactHideValue, HandExactHideChange, L"shown|hidden"},
    {kGroupHand, L"Hide rule", L"Where the game's Hand hides", "hand_hide_all",
     HandExactHideAllValue, HandExactHideAllChange, L"exact|all"},
    {kGroupHand, L"Hand flow", L"Glow of a miracle on the hand",
     "hand_flow_glow", HandExactFlowValue, HandExactFlowChange, L"off|on"},
    {kGroupHand, L"Hand model", L"Model of the hands", "hand_model",
     HandModelSwapValue, HandModelSwapChange, L"low poly|game|file"},
    {kGroupHand, L"Hand look", L"Look of the hands", "", HandModelLookValue,
     HandModelLookChange, L"player|evil|neutral|good"},
    {kGroupHand, L"Controller fit", L"Controller on the grip pose", "grip_fit",
     GripFitValue, GripFitChange, L"runtime|model"},

    // Throw
    {kGroupThrow, L"Fast release", L"Release at once", "release_at_once",
     HandDirectReleaseAtOnceValue, HandDirectReleaseAtOnceChange, L"off|on"},
    {kGroupThrow, L"Throw law", L"Throw: root law or real flight",
     "throw_law", ThrowLawValue, ThrowLawChange, L"gain|root|real"},
    {kGroupThrow, L"Throw picture", L"Picture between the physics steps",
     "throw_draw", ThrowDrawValue, ThrowDrawChange, L"flight|real"},
    {kGroupThrow, L"Fall rule", L"Still hand lets a high object fall",
     "release_fall_cm", ThrowFallValue, ThrowFallChange, L"off|on"},
    {kGroupThrow, L"Landing", L"Landing of a thrown object", "throw_land",
     ThrowLandValue, ThrowLandChange, L"game|land|flight"},
    {kGroupThrow, L"Tree settle", L"A tree that falls settles",
     "throw_tree_settle", ThrowSettleValue, ThrowSettleChange, L"off|on"},
    {kGroupThrow, L"Throw turn", L"Turn of a thrown object at the game's pace",
     "throw_turn", ThrowTurnValue, ThrowTurnChange, L"game|pace"},
    {kGroupThrow, L"Land rest", L"Rest after a landing", "throw_land_rest",
     ThrowRestValue, ThrowRestChange, L"off|on|spin|steep|still"},
    {kGroupThrow, L"Throw slower", L"Time factor of a throw x10",
     "throw_slow_x10", ThrowSlowValue, ThrowSlowerChange, L"", nullptr,
     false},
    {kGroupThrow, L"Throw faster", L"Time factor of a throw x10",
     "throw_slow_x10", ThrowSlowValue, ThrowFasterChange, L"",
     ThrowSlowerChange, true, L"Throw speed"},
    {kGroupThrow, L"Land slower", L"Rate of the land x10",
     "throw_land_rate_x10", ThrowLandRateValue, ThrowLandSlowerChange, L"",
     nullptr, false},
    {kGroupThrow, L"Land faster", L"Rate of the land x10",
     "throw_land_rate_x10", ThrowLandRateValue, ThrowLandFasterChange, L"",
     ThrowLandSlowerChange, true, L"Land speed"},

    // Creature
    {kGroupCreature, L"Command rule", L"Rule of the Creature command",
     "creature_command_rule", CreatureCommandRuleValue,
     CreatureCommandRuleChange, L""},
    {kGroupCreature, L"Command look", L"Mark of a target of the trigger",
     "command_look", CreatureCommandLookValue, CreatureCommandLookChange,
     L"halo|ring"},
    {kGroupCreature, L"Hold stay", L"Long grip makes the Creature stay",
     "creature_hold_stay", CreatureStayModeValue, CreatureStayModeChange,
     L"off|on"},
    {kGroupCreature, L"Open stroke", L"Open hand strokes the Creature",
     "creature_stroke_open", ControllersOpenStrokeValue,
     ControllersOpenStrokeChange, L"grip only|open hand"},
    {kGroupCreature, L"Panel stays", L"Creature panel with no press",
     "creature_panel_linger", UiPanelLingerValue, UiPanelLingerChange,
     L"press only|stays"},
    {kGroupCreature, L"Stroke hand", L"Hand of a stroke",
     "creature_stroke_hand", HandDirectStrokeHandValue,
     HandDirectStrokeHandChange, L"by size|flat|finger|fist"},
    {kGroupCreature, L"Stroke limits", L"Limits of the hand of a stroke",
     "creature_finger_below_pct", HandDirectStrokeLimitsValue,
     HandDirectStrokeLimitsChange, L"ini|fixed"},
    {kGroupCreature, L"Palm point", L"Stroke point at the palm",
     "creature_palm_point", HandDirectPalmPointValue,
     HandDirectPalmPointChange, L"finger|palm"},
    {kGroupCreature, L"Slap speed", L"Speed of a slap", "creature_slap_mps",
     HandDirectSlapValue, HandDirectSlapChange,
     L"ini|0.7 m/s|1.0 m/s|1.3 m/s|1.6 m/s"},
    {kGroupCreature, L"Miracle hand", L"A miracle is in the real hand",
     "miracle_hand", MiracleHandValue, MiracleHandChange, L"off|on"},
    {kGroupCreature, L"Totem hand", L"The totem follows the real hand",
     "totem_hand", VillageTotemValue, VillageTotemChange, L"off|on"},
    {kGroupCreature, L"Hold target", L"Halo on the thing at the Hand",
     "hold_target", VillageHoldValue, VillageHoldChange, L"off|on"},
    {kGroupCreature, L"Scaffold turn", L"Wrist turns a scaffold building",
     "scaffold_wrist_turn", VillageWristTurnValue, VillageWristTurnChange,
     L"off|on"},
    {kGroupCreature, L"Spell book", L"X opens the spell book", "spell_book",
     SpellBookValue, SpellBookChange, L"off|on"},
    {kGroupCreature, L"Book pull", L"Pull a miracle out of the book",
     "spell_book_pull_cm", SpellBookPullValue, SpellBookPullChange,
     L"off|on"},
    {kGroupCreature, L"Book bubble", L"The game's bubble on the pull",
     "spell_book_bubble", SpellBookBubbleValue, SpellBookBubbleChange,
     L"flat|game"},
    {kGroupCreature, L"All miracles", L"All miracles, no cost (test)",
     "debug_all_miracles", SpellBookAllValue, SpellBookAllChange, L"off|on"},
    {kGroupCreature, L"Gesture point", L"Point that draws a gesture",
     "gesture_point", ControllersGesturePointValue,
     ControllersGesturePointChange, L"grip|fingertip"},
    {kGroupCreature, L"Gesture trail", L"Trail at the fingertip",
     "gesture_trail", OverlayTrailValue, OverlayTrailChange, L"off|on"},

    // Sound
    {kGroupSound, L"Sound step", L"Sound of the mod, step by step", "",
     AudioModeValue, [](const char*) { AudioCycleMode(); },
     L"as set|same dist|QMixer pan|game picks|game ears"},
    {kGroupSound, L"Stop wait", L"Wait of 20 ms at a sound stop",
     "audio_stop_wait", AudioStopWaitValue,
     [](const char*) { AudioStopWaitToggle(); }, L"skip|wait"},
    {kGroupSound, L"Take loop", L"Ears start the loop of a take",
     "audio_take_loop", AudioTakeLoopValue, AudioTakeLoopChange, L"off|on"},

    // Temple
    {kGroupTemple, L"Temple pointer", L"Pointer and click in the Temple",
     "temple_pointer", TemplePointerValue, TemplePointerChange,
     L"ray click|ray plain|hand"},
    {kGroupTemple, L"Room flight", L"Camera flight into a room",
     "temple_camera_flight", TempleFlightValue, TempleFlightChange,
     L"off|on"},
    {kGroupTemple, L"Room view", L"View of a room of the Temple",
     "temple_view", TempleViewValue, TempleViewChange,
     L"screen|head view|game cam"},
    {kGroupTemple, L"Temple fade", L"Fade of the Temple in both eyes",
     "temple_fade_eyes", TempleFadeValue, TempleFadeChange,
     L"left eye|both eyes"},
};


// THE NOTES of the debug window: what each switch is and what it does, from
// the key tables of the docs. One line for each line of kSwitches, in the
// same order, so a new switch gets its note here.
const wchar_t* const kTips[] = {
    // Table
    L"Turns the table, or the God view, on and off. When it comes back, it "
    L"faces the game camera again.",
    // Follow camera
    L"On: the player goes where the game puts its camera. Off: the view moves "
    L"only at a start, a load and the end of a story.",
    // Camera place
    L"Recenter: the game camera is over the recenter place. Head: it is over "
    L"the land point under the head.",
    // Smooth zoom
    L"Steps: one zoom step for each push of the stick. Smooth: the scale "
    L"changes while the stick is held.",
    // Zoom point
    L"The point that stays in place in the room during a zoom: the land under "
    L"the head, the land that the head looks at, or the point of the Hand.",
    // Stick walk
    L"The stick of the hand that does not point. Head: forward is where the "
    L"head looks. Controller: where the controller points. Off: no walk.",
    // Move kind
    L"Walk: the view follows the land under the head. Walk level: the view "
    L"keeps its height. Fly: the stick moves the view along the forward "
    L"direction in 3D, and the view does not follow the land.",
    // Speed ramp
    L"On: after 1 second of a full push of the walk stick the speed doubles "
    L"each 2 seconds, up to 32 times. A smaller push goes back to its own "
    L"speed at once. Off: the speed of the push only.",
    // Fly at solids
    L"Slide: a fly slides past a house, a rock or a tree trunk, and only a "
    L"push straight at it stops. Stop: a fly stops 30 cm short of it.",
    // Vignette
    L"The edges of the view go dark while a control moves the view: a walk, "
    L"a zoom, a turn, a teleport or a grab of the world. Off, gentle or "
    L"strong.",
    // Head blinder
    L"On: when the head goes into the land, a gold and red overlay covers "
    L"the view. It starts at the edge 5 cm over the land. Off: the eyes see "
    L"under the land.",
    // Blinder solids
    L"Solids too: a house, a rock and a tree trunk cover the view as the "
    L"land does when the head goes into them. Land only: the eyes see into "
    L"a house and a rock.",
    // Menu turn
    L"Stops: while a dialog box of the game is up, the stick does not turn "
    L"or zoom. Turns: the stick turns and zooms as at other times.",
    // Recenter hold
    L"A hold of the stick click for 1 s, with the stick at rest, recenters "
    L"the view. Other hand: the click of the hand that does not point. Hand: "
    L"the click of the pointing hand. Off: no hold recenters.",
    // Grab test
    L"A test step for the grab of the world: as the ini sets it, one to one, "
    L"or off. A grab that holds ends, and the next press uses the step.",
    // Ring size
    L"Room: the teleport ring has one size in the room. World: it has one "
    L"size in the world.",
    // Jump switch
    L"The game switch that a press of A for the teleport keeps to. Jump: the "
    L"double click jump. Drag: the drag of the land, as the stick walk.",
    // Camera zone
    L"Game: the game pushes its camera back into a script zone, with its "
    L"sound and its shake. View: the God view stays in the zone, with no "
    L"shake.",
    // Button layout
    L"The buttons of the hand that does not point. Ring on X: Y opens the "
    L"game menu, X and the stick open the ring. Ring on Y: Y opens the ring.",
    // Ring ways
    L"What opens the ring: both ways, the button only (the hand picks), or "
    L"the stick only (the trigger chooses). Only when the stick can drive the "
    L"ring.",
    // Pointer owner
    L"Automatic: the mod picks the owner of the pointer. Controller: only the "
    L"controllers move the pointer. Mouse: only the mouse moves it.",
    // Script player
    L"Camera: a script that measures a distance from the camera uses the game "
    L"camera. Player: it uses the place of the player.",
    // Deep zoom
    L"For a land more than 2.5 m below the head. Land level: a zoom in keeps "
    L"the land at its height in the room. 2.5 m level: a zoom in moves the "
    L"land farther down. A zoom out brings that land up in both.",
    // Scene window
    L"On: a cut scene of the God view plays on a screen in the room. Off: it "
    L"plays in the full view, with held shots.",
    // Scene depth
    L"Off: both eye pictures of the cut scene are at one depth. Nearest: the "
    L"nearest thing stays on or behind the screen. Fixed: the game near "
    L"plane.",
    // Eye clean
    L"Own: each eye pass starts from its own frame before. Shared: the two "
    L"eye passes share the render target.",
    // Walk marks
    L"In one walk. On: bubbles, name cards, the Hand glow and the halos draw "
    L"for each eye alone. Off: the right eye gets them as other batches.",
    // Land light
    L"A light on the land that comes one time a frame, such as the light of a "
    L"lightning. It shows in both eyes, or in the left eye only.",
    // Sea depth
    L"Depth: the land of the sea goes down under the water and fades into "
    L"the sea by its depth. Flat: the flat coast of the game.",
    // Cull distance
    L"How far out the map objects are drawn: trees, houses and villagers. "
    L"Game: 700 units, as in the game. Each step multiplies it.",
    // Cull fog
    L"Follows: the fog starts and ends further out, as far as the cull "
    L"distance moves the map objects out. Game: the fog of the game.",
    // Edge look
    L"The glow at the edge of the influence near a hand. Hologram: a red grid "
    L"on the wall of the edge, with rings from the hand. Patch: a round glow.",
    // Advisor size
    L"An advisor at a place of the world. Near size: the size that it has for "
    L"the eye at its usual place. Game: the size of the game.",
    // Vulkan eyes
    L"In a Vulkan session, the eye pictures come from the Vulkan renderer or "
    L"from Direct3D 9. The change starts at the next frame.",
    // Keep picture
    L"Vulkan only. Keep: a walk with no clear of the whole target starts with "
    L"the picture of the walk before. Black: each walk starts black.",
    // Stage 1
    L"On: the Vulkan renderer uses texture stage 1, for example the light "
    L"maps of the Temple. Off: it uses stage 0 alone.",
    // No texture
    L"A Vulkan stage 0 with no texture. Diffuse: it uses the diffuse color, "
    L"as Direct3D does. White: it reads a white texel.",
    // Ray glow
    L"On: the menu ray, the far grab tether and the teleport arc glow, with a "
    L"light that runs from the hand. Off: plain tubes.",
    // Glow only
    L"Glow: with Ray glow on, the glow alone is the ray, narrower and with a "
    L"solid core. Tube: the glow is over a thin tube.",
    // Ray on panels
    L"Over: while a panel in the room is open, the ray shows over the panel. "
    L"Under: the panel hides the ray.",
    // Ray tip
    L"Glow: the end of the menu ray is a round soft glow that faces you. It "
    L"has no outline. Mark: a small cross, or a disc in a dark ring over a "
    L"panel.",
    // Video view
    L"On: the game window on the PC shows a 16:9 part of the left eye for a "
    L"video capture, with a smoothed head. Only with the Vulkan renderer.",
    // Video marks
    L"On: faint corner marks in the headset show the part of the view that "
    L"the video records. The video does not show them.",
    // Video layers
    L"On: the video also shows the cards, the ring and the panels of the "
    L"headset, where the left eye sees them. Off: the eye picture only.",
    // Video higher
    L"The part that the video records goes 5 percent of the height up.",
    // Video lower
    L"How far the middle of the video is below the middle of the view, in "
    L"percent of the height. The right arrow moves it 5 percent down, the "
    L"left arrow 5 percent up.",
    // One walk
    L"Walks the scene of the game one time for both eyes. Off walks it one "
    L"time for each eye, which costs more time.",
    // Foveation
    L"On: fewer shades away from the center of the Vulkan eye pictures. "
    L"Marks: also a color over the coarse parts. For a test.",
    // Fovea images
    L"On: each Vulkan eye picture comes from an outer image and a center "
    L"image, each with its own resolution.",
    // Frame send
    L"When the mod waits for the GPU. Each eye: after each eye pass. One "
    L"wait: one time a frame. 2nd thread: one wait a frame on a second "
    L"thread.",
    // Frame hold
    L"When the game gives no frame for this many ms, a thread sends the last "
    L"frame again. 0 is off. A step goes to 0 and back.",
    // Real time
    L"Real: after the eye passes, the game gets the real time of the frame. "
    L"Kept: it gets the time from before the passes.",
    // SSE loops
    L"The two vertex loops of the engine run with SSE or on the x87 unit. The "
    L"change starts at the next mesh.",
    // Thread wait
    L"Sleep: a thread of the mod that waits spins for a short time, then "
    L"sleeps. Spin: the threads spin for the whole walk.",
    // Read ahead
    L"On: a small read from a game file comes from a copy of the next 256 KB. "
    L"Off: each read goes to Windows, as in the plain game.",
    // Follow target
    L"On: the Vulkan renderer follows the render target, the viewport and the "
    L"clears. Off: each draw goes into the eye pictures, with no clear.",
    // Upload queue
    L"Queue: a texture that a walk reads again goes to the GPU in the submit, "
    L"with no wait. Wait: each copy goes at once and waits for the GPU.",
    // Vulkan window
    L"With the Vulkan backend, the game window gets its picture through a "
    L"Vulkan swapchain, or through the CPU and GDI.",
    // Vulkan only
    L"On: a walk with the Vulkan eyes goes to the Vulkan renderer alone. Off: "
    L"Direct3D 9 draws each walk too.",
    // Vulkan upload
    L"In a Vulkan session, the CPU pixels (cards, CPU eye pictures, cut scene "
    L"walls) go into the swapchains by a Vulkan copy, or through Direct3D 11.",
    // Render thread
    L"On: the calls of each walk go to the render thread. Kept: the mod keeps "
    L"them until each stop, for a test only. Off: each call goes as it comes.",
    // No lock
    L"On: the mod answers some texture calls with no lock of the device. Off: "
    L"each call goes to Direct3D 9. Needs the render thread on in the ini.",
    // Call filter
    L"On: a call that sets a value that the device has already does not go to "
    L"Direct3D 9. Needs the render thread on in the ini.",
    // Own buffers
    L"On: the render thread draws from buffers of its own. Off: each draw "
    L"goes to Direct3D 9 as it is. Needs the render thread on in the ini.",
    // Merge eyes
    L"On: a batch of one walk is one draw for both eyes. Off: one draw for "
    L"each eye. Needs the render thread on in the ini.",
    // GPU flush
    L"The driver gives the calls of a walk to the GPU each N draws. 0: when "
    L"the driver wants. A step goes to 0 and back.",
    // Right eye copy
    L"The thread that makes the right eye copy of a batch in one walk. "
    L"Render: the render thread. Frame: the frame thread.",
    // D3D7 thread
    L"On: the Direct3D 7 calls of a walk go to their own thread. Off: to "
    L"dxwrapper on the frame thread. Needs the render thread on in the ini.",
    // D3D7 merge
    L"On: the D3D7 thread merges a row of draws with one state. Off: the "
    L"frame thread merges them.",
    // Late end
    L"No wait: the end of a walk does not wait for the render thread. Wait: "
    L"it waits.",
    // UI layer
    L"In picture: the interface is in the eye pictures. Own layer: it is on "
    L"its own layers. Test: both, and it saves captures. Desktop only.",
    // Subtitle angle
    L"How far below the center of the view the subtitles are, in degrees. A "
    L"step goes 25, 30, 35, 40, 45, then 25 again.",
    // Subtitle copy
    L"No wait: the mod reads the copy of the subtitle card in a later frame. "
    L"Wait: it reads it in the same frame and waits for the GPU.",
    // Bubble card
    L"On: a speech bubble of the game is one flat card at the depth of its "
    L"sign. Off: the game draws it in each eye pass.",
    // Note depth
    L"On: a note of the game over a thing, for example a lesson percent over "
    L"the Creature, has the depth of its place. Off: the interface depth.",
    // Hint depth
    L"On: a tooltip or a hint of the game at the Hand has the depth of the "
    L"Hand in both eyes. Off: the same pixel in both eyes.",
    // Vulkan panel
    L"On: the Vulkan renderer draws the panel for the subtitle card and the "
    L"panel layer. Off: they use the panel of Direct3D 9.",
    // Prompt timing
    L"On: the prompt of a target shows at once, stays while the target is "
    L"there, and shows each time. Off: all prompts use the dwell, hold and "
    L"count.",
    // Hint words
    L"0: the hint of the game replaces the prompt. 1: the prompt of a target "
    L"owns its button. 2 and 3: the prompt of a take, then a fetch, keeps the "
    L"grip.",
    // Prompt moves
    L"On: the walk line shows while a push moves the player, and the A line "
    L"only when A can move the player. Off: no walk line.",
    // Card look
    L"Scroll: the cards, the ring, the spell book and the keyboard are "
    L"parchment scrolls with wax seals. Plain: dark cards with white letters.",
    // Card pictures
    L"On: a line of a button has a small picture of the controller with that "
    L"button lit. Off: the button word only.",
    // Tree card
    L"On: the card says what an open grip does with a tree in the Hand. Off: "
    L"the card shows the text of the game, Plant, each time.",
    // Text click
    L"On: while a text of the game waits for a click, the card shows its "
    L"prompt and the trigger clicks it. Off: the usual rule of a hint.",
    // Catch moving
    L"Extra reach for an object that moves. Pick: on the pick reach only. "
    L"Rest: on all of the reach at rest. Open: as Rest, the guard ends at an "
    L"open grip.",
    // Wide pick
    L"Reach in cm for an object that the Hand can hold, when nothing nearer "
    L"is a target. 0 is off. A step goes to 0 and back.",
    // Near rule
    L"The pick at the Hand. One target: the rule of the mod. Nearest: the "
    L"nearest body in reach. Old: a reach of 60 percent of the Hand, no near "
    L"rules.",
    // Far choice
    L"The choice in the far grab cone. Point: one point. Bias: living things "
    L"score better. Ranks: living things first. Balls: a ball for each "
    L"object.",
    // Far stay
    L"On: a living thing with the halo keeps it against things that are not "
    L"living, while the aim stays near its cone. Off: a margin of 2 degrees "
    L"only.",
    // Take carry
    L"On: when the source of a far take goes away while the grip is closed, "
    L"the grip carries the bundle. Off: the bundle drops at once.",
    // Roots pile
    L"Roots: the roots pile of a far tree goes where the tree stood. Glide: "
    L"it stays where the game puts it, on the way of the glide.",
    // Take stream
    L"Curve: the pieces of a far take go on a curve from the top of the "
    L"source to the Hand. Line: the line of the game, from the land under the "
    L"Hand.",
    // Forest take
    L"On: a closed grip collects the trees of a forest in one bundle. Off: "
    L"one tree for each grip, as in the plain game.",
    // Target glow
    L"What marks a target of the Hand. Mesh: a glow on its mesh, or a halo "
    L"when the mesh cannot glow. Halo: a halo. Both: the glow and a halo.",
    // Idle glow
    L"On: the glow shows also with no target, and while the Hand holds an "
    L"object. Off: the glow shows only on a target.",
    // Scroll draw
    L"On: each scroll that the game draws is a target, with the ball of its "
    L"draw. Off: only what the pick of the game offers.",
    // Press guard
    L"On: in the first 300 ms of a press, before the Hand holds the object, a "
    L"closed grip opens only below 35 percent. Off: it can open at any time.",
    // Press hold
    L"On: the game button stays down until the game has the object in its "
    L"Hand. Off: it goes up 167 ms after the object comes into the Hand.",
    // Take keeps
    L"On: while a bulk take runs, only a grip below 35 percent ends it. Off: "
    L"a fast fall of the grip ends the take too.",
    // Take guard
    L"On: while the Hand takes an object, the mod holds back the end of its "
    L"physics, so a tree stays a tree. Off: the tree can die, and the Hand is "
    L"empty.",
    // Catch at once
    L"On: a grip on an object that moves puts it in the Hand at once. Off: "
    L"the game picks it up 225 ms later, and it moves on until then.",
    // Pinch
    L"Pinch: the Hand holds a small object between the thumb and the index "
    L"finger. Fist: the Hand holds it in the fist.",
    // Finger feel
    L"On: the fingers react to the world. Off: the fingers have the curls of "
    L"the pose only.",
    // Land stop
    L"On: a hand that goes into the land stays on the land and slides along "
    L"it. Off: the land hides the hand, and only the point stays on it.",
    // Object stop
    L"On: a hand that goes into a house, a rock or a tree trunk stays on its "
    L"surface and slides along it. Off: the hand goes into them.",
    // Game Hand
    L"Hidden: the mod hides the mesh of the game Hand while a controller "
    L"places it, and draws both hands. Shown: the game draws its Hand too.",
    // Hide rule
    L"All: the game Hand hides in each state, also at the totem, and the "
    L"copy with the glow of a miracle hides too. Exact: it hides only where "
    L"it follows the real hand.",
    // Hand flow
    L"On: while a miracle is in the Hand, your hand has the moving glow that "
    L"the game puts on its Hand. Off: no glow. It needs the hide rule All.",
    // Hand model
    L"The hands that the mod draws. Low poly: simple hands. Game: the hand "
    L"mesh of the game. File: the hand mesh from a file.",
    // Hand look
    L"The look of the game hand mesh: the alignment of the player, evil, "
    L"neutral or good. The low poly hands do not change.",
    // Controller fit
    L"Model: on Virtual Desktop, the Oculus runtime and SteamVR, the drawn "
    L"controller, the hands and the Hand go on the frame of the model of a "
    L"Meta Touch controller, 2.7 cm from the grip pose. Runtime: on the grip "
    L"pose of the runtime.",
    // Fast release
    L"On: a game turn runs on the frame after a release of the Hand, or after "
    L"the grip opens in a take. Off: the mod waits up to 100 ms for the game "
    L"turn.",
    // Throw law
    L"Real: a throw has the real flight. Root: the square root law with the "
    L"game physics. Gain: a fixed gain with the game physics.",
    // Throw picture
    L"Where the mod draws a thrown object between game steps. Real: a substep "
    L"with a touch is 5 ms, and the picture waits at a guess. Flight: flight "
    L"time.",
    // Fall rule
    L"On: a still hand lets an object fall when the Hand point is high over "
    L"the land. Off: the hand sets the object down at each height.",
    // Landing
    L"Game: the landing of the game. Land: the land rate, with a low bounce "
    L"and the brake. Flight: the low bounce and the brake at the flight rate.",
    // Tree settle
    L"On: a falling tree that touches the land at a low speed turns down "
    L"fast. Off: the turn of the game. Needs a landing of the mod.",
    // Throw turn
    L"Pace: a thrown object turns as fast as the game turns it. Game: the "
    L"momentum of the game, so the picture turns faster in a slow flight.",
    // Land rest
    L"How a landed object comes to rest. On: three rules (spin, steep land, "
    L"still). Spin, Steep, Still: that rule only. Off: the rules of the game.",
    // Throw slower
    L"Makes the time factor of a throw larger, so the next throw flies "
    L"slower.",
    // Throw faster
    L"The time factor of a throw, times 10, from 10 to 100. The next throw "
    L"has the real arc, this many times slower. Left: slower. Right: faster.",
    // Land slower
    L"Makes the rate of the land smaller, so a thrown object moves slower "
    L"after it lands.",
    // Land faster
    L"How fast a thrown object moves after its first contact, times 10, from "
    L"10 to 100. 10 is the time of the game. Left: slower. Right: faster.",
    // Command rule
    L"4: a press on the land stays a walk. 3 and 2: a press goes to a live "
    L"target. 1: the press fixes its kind. A step goes down to 1, then to 4.",
    // Command look
    L"Ring: a target of the trigger has a green ring on the land, and a glow "
    L"shows only what the grip acts on. Halo: a target of the trigger has a "
    L"green halo.",
    // Hold stay
    L"On: a long grip on the far Creature makes it stay. Off: the leash click "
    L"goes out when the grip closes, and no grip makes it stay.",
    // Open stroke
    L"On: an open hand that stays 0.2 s in the body of your Creature strokes "
    L"it with no grip. The open hand never slaps. Off: only the grip strokes.",
    // Panel stays
    L"On: the Creature panel shows while the Hand is at your Creature with no "
    L"press, and for 3 s after the Hand goes. Off: only a press shows it.",
    // Stroke hand
    L"By size: the size of the Creature chooses the hand of a stroke. Flat: "
    L"the flat hand always. Finger: the index finger always. Fist: the fist.",
    // Stroke limits
    L"The size limits of the finger and the flat hand in a stroke. Ini: the "
    L"limits of the ini. Fixed: the fixed limits of the test key.",
    // Palm point
    L"Palm: with the flat hand of a stroke, the point of the Hand is the "
    L"palm. Finger: it is the tip of the index finger.",
    // Slap speed
    L"The speed of the hand that a slap needs. Ini: the value of the ini. A "
    L"hard slap needs 1.8 times this speed.",
    // Miracle hand
    L"On: a miracle is in the real hand, with a target, a cancel and lines. "
    L"Off: the game places the Hand with the miracle.",
    // Totem hand
    L"On: the totem follows the real hand. Off: the game moves the totem with "
    L"the screen rows of the pixel of the Hand point.",
    // Hold target
    L"On: with an object in the Hand, the target is the thing at the Hand, "
    L"with a halo. Off: the object behind the pixel of the Hand point.",
    // Scaffold turn
    L"On: with a scaffold in the Hand, a roll of the wrist turns the building "
    L"one to one, and the building does not turn by itself. Off: the "
    L"building turns by itself, as in the game.",
    // Spell book
    L"On: the first face button of the hand that does not point opens the "
    L"spell book. Off: that button has no job.",
    // Book pull
    L"On: the hand pulls a miracle out of the book. Off: the grip takes the "
    L"miracle at once.",
    // Book bubble
    L"Game: the bubble that the hand pulls out is the bubble of the game. "
    L"Flat: a flat picture with the two gestures.",
    // All miracles
    L"For a test only. Each town knows each miracle, the spell book shows up "
    L"to 6, and the miracles cost no prayer power.",
    // Gesture point
    L"Fingertip: the tip of the index finger draws a gesture. Grip: the grip "
    L"point draws it.",
    // Gesture trail
    L"On: a ribbon follows the point of the draw while the draw button is "
    L"held. Off: no trail.",
    // Sound step
    L"A test. Each step removes more of the mod sound: as set, same distance "
    L"at each zoom, QMixer pans, the game picks the sounds, the game ears.",
    // Stop wait
    L"Skip: the mod skips the 20 ms wait of the audio DLL at a sound stop. "
    L"Wait: each stop holds the frame thread for 20 ms.",
    // Take loop
    L"On: the ears start the loop sound of a bulk take. Off: the game camera "
    L"starts it, and in the God view the take has no sound.",
    // Temple pointer
    L"Ray click: a ray, and the trigger and the grip are both Action. Ray "
    L"plain: a ray with the plain buttons. Hand: the real hand, with no line.",
    // Room flight
    L"On: the camera flies into a room of the Temple, as in the plain game. "
    L"Off: the camera is at its rest pose at once.",
    // Room view
    L"Head view: each eye draws the room from the head. Screen: the same, but "
    L"the head does not turn the room camera. Game cam: the camera of the "
    L"game.",
    // Temple fade
    L"Both eyes: the fade of the Temple shows in both eyes. Left eye: it "
    L"shows in the left eye only.",
};

const int kCount = static_cast<int>(sizeof(kSwitches) / sizeof(kSwitches[0]));
static_assert(sizeof(kTips) / sizeof(kTips[0]) == sizeof(kSwitches) /
                                                     sizeof(kSwitches[0]),
              "each switch needs one note in kTips");
static_assert(kCount <= radial_math::kMaxSwitches,
              "the ring has no item code for so many switches");

bool IniPath(char path[MAX_PATH]) {
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return false;
  char* slash = strrchr(path, '\\');
  if (slash == nullptr) return false;
  slash[1] = 0;
  if (strlen(path) + 9 >= MAX_PATH) return false;
  strcat_s(path, MAX_PATH, "bwvr.ini");
  return true;
}

// The two keys are read one time. TestSwitchesOn runs in DllMain too, before
// the other keys of the mod are read, so it reads the file itself.
int g_test_keys = -1;
int g_debug_menu = -1;
int g_debug_window = 1;

void ReadKeys() {
  if (g_debug_menu >= 0) return;
  char path[MAX_PATH];
  if (!IniPath(path)) {
    g_test_keys = 0;
    g_debug_menu = 0;
    return;
  }
  g_test_keys = GetPrivateProfileIntA("bwvr", "test_keys", 0, path) != 0;
  g_debug_menu = GetPrivateProfileIntA("bwvr", "debug_menu", 0, path) != 0;
  g_debug_window =
      GetPrivateProfileIntA("bwvr", "debug_window", 1, path) != 0 ? 1 : 0;
}

void Narrow(const wchar_t* in, char* out, size_t size) {
  size_t n = 0;
  for (; n + 1 < size && in[n] != 0; ++n) {
    out[n] = (in[n] >= 32 && in[n] < 127) ? static_cast<char>(in[n]) : '?';
  }
  out[n] = 0;
}

}  // namespace

bool TestSwitchesOn() {
  ReadKeys();
  return g_test_keys != 0 || g_debug_menu != 0;
}

bool DebugMenuOn() {
  ReadKeys();
  return g_debug_menu != 0;
}

int DebugGroupCount() { return kGroups; }

const wchar_t* DebugGroupName(int group) {
  return group >= 0 && group < kGroups ? kGroupName[group] : L"";
}

int DebugSwitchCount() { return kCount; }

int DebugSwitchValue(int i) {
  if (i < 0 || i >= kCount) return kDebugCannot;
  return kSwitches[i].value();
}

int DebugSwitchGroup(int i) {
  if (i < 0 || i >= kCount) return -1;
  return kSwitches[i].group;
}

const wchar_t* DebugSwitchLabel(int i) {
  return i >= 0 && i < kCount ? kSwitches[i].label : L"";
}

const wchar_t* DebugSwitchName(int i) {
  return i >= 0 && i < kCount ? kSwitches[i].name : L"";
}

const char* DebugSwitchKey(int i) {
  return i >= 0 && i < kCount ? kSwitches[i].key : "";
}

void DebugSwitchValueText(int i, int value, wchar_t* out, int size) {
  if (size <= 0) return;
  out[0] = 0;
  if (i < 0 || i >= kCount) return;
  if (value == kDebugCannot) {
    swprintf_s(out, size, L"-");
    return;
  }
  if (debug_math::ValueWord(kSwitches[i].words, value, out, size) > 0) return;
  swprintf_s(out, size, L"%d", value);
}

namespace {

// A step of switch i: its change function, or `back` for the left arrow of
// the window. The log has the switch and its new value.
bool Step(int i, bool back) {
  if (i < 0 || i >= kCount) return false;
  const Switch& s = kSwitches[i];
  const int before = s.value();
  char label[48];
  Narrow(s.label, label, sizeof(label));
  if (before == kDebugCannot) {
    ModLog("debug page   : the player chose \"%s\", and it cannot change now, "
           "frame %llu", label, ControllersFrameCount());
    return false;
  }
  const bool other = back && s.back != nullptr;
  (other ? s.back : s.change)(kDebugBy);
  const int after = s.value();
  wchar_t word[32];
  char text[32];
  DebugSwitchValueText(i, after, word, 32);
  Narrow(word, text, sizeof(text));
  const char* way = other ? " (a step back)" : "";
  if (s.key[0] != 0) {
    ModLog("debug page   : the player chose \"%s\"%s: %s is %d now (%s), it "
           "was %d, frame %llu", label, way, s.key, after, text, before,
           ControllersFrameCount());
  } else {
    ModLog("debug page   : the player chose \"%s\"%s: the value is %d now "
           "(%s), it was %d, frame %llu", label, way, after, text, before,
           ControllersFrameCount());
  }
  return true;
}

}  // namespace

bool DebugSwitchChoose(int i) { return Step(i, false); }

bool DebugWindowOn() {
  ReadKeys();
  return g_debug_menu != 0 && g_debug_window != 0;
}

bool DebugSwitchInWindow(int i) {
  return i >= 0 && i < kCount && kSwitches[i].in_window;
}

const wchar_t* DebugSwitchWindowLabel(int i) {
  if (i < 0 || i >= kCount) return L"";
  return kSwitches[i].window_label != nullptr ? kSwitches[i].window_label
                                              : kSwitches[i].label;
}

const wchar_t* DebugSwitchTip(int i) {
  return i >= 0 && i < kCount ? kTips[i] : L"";
}

int DebugSwitchValueCount(int i) {
  if (i < 0 || i >= kCount) return 0;
  const wchar_t* w = kSwitches[i].words;
  if (w == nullptr || w[0] == 0) return 0;
  int n = 1;
  for (; *w != 0; ++w) {
    if (*w == L'|') ++n;
  }
  return n;
}

bool DebugSwitchHasBack(int i) {
  return i >= 0 && i < kCount && kSwitches[i].back != nullptr;
}

bool DebugSwitchStep(int i, bool back) { return Step(i, back); }

void DebugMenuLogStart() {
  if (!DebugMenuOn()) return;
  int can = 0;
  for (int i = 0; i < kCount; ++i) {
    if (kSwitches[i].value() != kDebugCannot) ++can;
  }
  ModLog("debug page   : debug_menu=1. The ring has the item \"debug\" with "
         "%d switches in %d groups, %d of them can change now. test_keys=%d",
         kCount, static_cast<int>(kGroups), can, g_test_keys);
  int backs = 0;
  for (int i = 0; i < kCount; ++i) {
    if (kSwitches[i].back != nullptr) ++backs;
  }
  ModLog("debug page   : debug_window=%d. %s", g_debug_window,
         g_debug_window != 0
             ? "The item \"debug\" opens the debug window, and the ray and "
               "the trigger of the pointing hand change its switches"
             : "The item \"debug\" goes to the debug pages of the ring");
  if (g_debug_window != 0) {
    ModLog("debug page   : in the window, %d switches step back with the left "
           "arrow, and the left arrow of each other switch of more than two "
           "values makes the step of its test key", backs);
  }
}
