// THE SETTINGS OF THE PLAYER (docs/launcher.md, "The table").
//
// One table for each place that edits bwvr.ini: the launcher of the PC
// (src/launcher/launcher.cpp) and the menu of the headset app
// (android/src/menu_model.h). A row gives the key, the label, the kind of
// control, the default and the help text. This file has no Windows code, so
// the Android build uses it too.
#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace bwvr_settings {

enum Kind {
  kGroup,       // a heading, not a setting
  kBool,        // a checkbox
  kInt,         // a number
  kChoice,      // one of a list, stored as the index
  kValues,      // one of a "label=value" list, stored as the value
  kScale,       // a slider, as a percentage of what the headset asks for
  kSlider,      // a slider with a value of its own: "low|high|step|unit"
  kRenderer,    // the renderer: "backend" in the ini, as text
};

struct Setting {
  Kind kind;
  const char* key;    // the ini key, or null for a heading
  const char* label;
  const char* choices;  // "one|two|three" for kChoice
  int fallback;
  const char* help;
};

// THE WHOLE USER INTERFACE. Add a row, get a control.
//
// Only settings with more than one right answer are here. Settings that only
// break the game when changed are not offered, and SaveAll removes them from
// the ini (see kRetiredKeys).
//
// A row in a comment is a setting that the launcher does not offer now. The
// mod still reads its key, with the same default. To offer it again, take
// the comment marks away, and take its key out of kRetiredKeys if it is
// there.
inline const Setting kSettings[] = {
    {kGroup, nullptr, "Picture", nullptr, 0, nullptr},
    // Before the resolution: a save writes the rows in order, and the size of
    // the render reads this key.
    {kBool, "one_walk", "One scene walk for both eyes (prototype)", nullptr,
     1,
     "The game draws the scene one time, for the left eye, and the mod makes "
     "the right eye from the same drawing. This takes about 40 percent off "
     "the time of the scene. The render is a quarter wider, so that the "
     "right eye has a picture at its outer edge. A test build: report what "
     "looks wrong."},
    // Before the resolution too: the size of the render reads this key.
    {kValues, "fov_margin_pct", "Margin around the view",
     "None=0|5%=5|10%=10|15%=15|20%=20|25%=25|30%=30", 0,
     "If you see black at the edge of the view when you turn your head fast, "
     "set a margin. The game then draws that much more on each side, and the "
     "picture has that many more pixels each way, so it stays as sharp. A "
     "margin costs frame time: 20% is almost half as many pixels again."},
    {kScale, "resolution_pct", "Resolution scale", "50|200|5", 100,
     "How big a picture the game draws for each eye, as a percentage of what "
     "your headset asks for. The headset's own figure is read when the game "
     "runs, and shown beside the slider. Note that the mod renders a wider "
     "view than the headset needs, so 100% is a little softer than native and "
     "about 115% matches it."},
    {kValues, "anisotropy", "Anisotropic filtering",
     "Off=1|2x=2|4x=4|8x=8|16x=16", 16,
     "Sharpens textures seen at a shallow angle, such as the ground. 16x is "
     "the sharpest and costs almost nothing here."},
    {kBool, "force_mips", "Enable texture mipmaps", nullptr, 1,
     "Uses smaller copies of each texture in the distance. This removes most "
     "of the shimmer on far land and water."},
    {kChoice, "detail_distance", "Model detail distance", "Game|2x|3x|5x", 3,
     "How far out trees, villagers and buildings keep their full models "
     "before they change to simpler ones or disappear. Further costs more "
     "frame time in busy places."},

    {kGroup, nullptr, "Performance", nullptr, 0, nullptr},
    {kRenderer, "backend", "Renderer", "Vulkan|Direct3D 9", 0,
     "Vulkan (the default): the mod draws the game with a Vulkan renderer of "
     "its own, and the pictures go to the headset and to the window through "
     "Vulkan. The mod then uses no dxwrapper, no Direct3D 9 and no Direct3D "
     "11. Your GPU needs Vulkan 1.3: with no Vulkan 1.3 the launcher keeps "
     "Direct3D 9. "
     "Direct3D 9: the game draws through dxwrapper and Direct3D 9, as in "
     "the versions before. The change counts from the next start."},
    {kBool, "d3d9_backend", "Direct3D 9 through DXVK", nullptr, 0,
     "Only with the renderer Direct3D 9. The game draws through DXVK and "
     "Vulkan in place of the Direct3D 9 of Windows. This takes about half "
     "of the time of the driver off each frame. The mod uses it only when "
     "your GPU has Vulkan 1.3 and the file dxvk\\d3d9.dll is in the game "
     "folder. Else, and when a start with it fails, the game draws as "
     "before. The change counts from the next start."},
    {kBool, "xr_vulkan", "Headset session with Vulkan", nullptr, 0,
     "Only with the renderer Direct3D 9: the renderer Vulkan always has a "
     "session with Vulkan. The headset gets the pictures through Vulkan in "
     "place of Direct3D 11, and the mod draws the scene of each eye with its "
     "Vulkan renderer. Your GPU needs Vulkan 1.3. When the session with "
     "Vulkan does not start, the mod starts one with Direct3D 11. The change "
     "counts from the next start."},

    {kGroup, nullptr, "Foveated rendering", nullptr, 0, nullptr},
    {kBool, "foveation", "Fewer shades away from the middle", nullptr, 0,
     "The middle of each eye picture keeps all its shades of color, and the "
     "parts around it get one shade for 2 by 2 pixels (4 by 4 at the edge "
     "when the GPU has it). The edges of things stay sharp. This can take "
     "time off a GPU that is slow for each pixel. With an eye tracker (for "
     "example Steam Frame or Quest Pro) the middle follows your gaze. With "
     "no eye tracker (for example Quest 3) it stays in the middle of the "
     "view. It needs the Vulkan renderer, or the headset session with "
     "Vulkan, and a GPU with variable rate shading."},
    {kSlider, "foveation_inner_deg", "All shades within", "10|45|1| degrees",
     22,
     "How far the part with all shades goes from the middle to each side. "
     "Smaller saves more."},
    {kSlider, "foveation_outer_deg", "One shade for 2 by 2 within",
     "15|70|1| degrees", 35,
     "Past this angle a shade is for 4 by 4 pixels, when the GPU has it. It "
     "is at least the angle above."},
    {kBool, "fovea_res", "Fewer pixels away from the middle", nullptr, 0,
     "The mod draws each eye picture from two pictures: one of the whole "
     "view with fewer pixels, and one of the middle with more. The middle "
     "follows your gaze with an eye tracker, and stays in the middle of the "
     "view with none. The three sliders below set the two resolutions and "
     "the size of the middle. It needs the Vulkan renderer, or the headset "
     "session with Vulkan."},
    {kSlider, "fovea_res_outer_pct", "Resolution of the whole view",
     "5|100|5|%", 50,
     "The pixels of the whole view, as a percentage of the eye picture on "
     "each side. 50% is a quarter of the pixels."},
    {kSlider, "fovea_res_center_pct", "Resolution of the middle", "50|200|5|%",
     100,
     "The pixels of the middle, as a percentage of the eye picture on each "
     "side. More than 100% makes the middle smoother than the eye picture "
     "alone."},
    {kSlider, "fovea_res_center_deg", "Size of the middle", "10|45|1| degrees",
     25,
     "How far the middle goes from its centre to each side. A larger middle "
     "is sharp further out and costs more frame time."},
    {kSlider, "fovea_res_center_down_deg", "Middle lower by", "0|15|1| degrees",
     0,
     "Moves the sharp middle down from straight ahead, for example to the "
     "land and the hands at your waist. Only with no eye tracker: a middle "
     "that follows your gaze stays where you look."},
    {kBool, "fovea_res_layer", "The middle as its own layer (headset app)",
     nullptr, 0,
     "Only in the headset app (Quest). The headset shows the picture of the "
     "middle as a layer of its own, at its own resolution, over the picture "
     "of the whole view. So a middle above 100% is sharper than the eye "
     "picture: at about 200% it has as many pixels as the panel in the middle "
     "of the lens. The picture of the whole view keeps its own resolution "
     "too. The eye picture keeps the size of the render, and holds the two "
     "pictures side by side, so the mod does not draw the whole eye picture "
     "from them. When the two pictures do not fit side by side, the middle "
     "gets fewer pixels. Off: the two pictures go into one eye picture. On "
     "the PC this does nothing. The change counts from the next start of the "
     "game."},
    {kValues, "foveation_gaze", "The middle follows",
     "Your gaze=1|Your gaze, OpenXR only=2|Your gaze, OSC only=3|"
     "Straight ahead=0", 1,
     "With an eye tracker the middle of both kinds follows your gaze. The "
     "mod reads the gaze from the headset (OpenXR), and else from OSC "
     "messages of the eye tracker. Steam Frame: in the SteamVR settings, "
     "Steam Link, let Steam Link share the eye tracking with other apps. "
     "For OSC, send it to 127.0.0.1 and the port below. Other eye trackers "
     "can send OSC with a tool such as VRCFaceTracking."},
    {kInt, "eye_osc_port", "OSC port of the gaze", nullptr, 9000,
     "The UDP port on which the mod receives the gaze as OSC (the addresses "
     "/tracking/eye/... of VRChat). Use the same port in the program that "
     "sends the gaze. VRChat uses 9000 too: when VRChat runs, give the mod "
     "and the sender another port. 0 receives nothing."},
    {kBool, "eye_osc_any", "Gaze from OSC of other devices", nullptr, 0,
     "Receive the OSC of the gaze from other devices of the network, for "
     "example from a program that runs on the headset. Off: only from this "
     "PC (127.0.0.1). Windows can ask if the mod may use the network."},
    {kBool, "foveation_show", "Show the parts with fewer details", nullptr, 0,
     "A blue colour over the parts with fewer shades or fewer pixels, so "
     "you can find the settings that you like. Turn it off to play. With "
     "the test keys, Shift O and Shift P change the two kinds while you "
     "play."},

    {kGroup, nullptr, "View", nullptr, 0, nullptr},
    {kValues, "play_mode", "Play mode",
     "God view=4|Stereoscopic window=1|Tabletop (test)=3", 4,
     "God view: the land is at your waist, so your hands can reach it. It "
     "works with motion controllers and with the mouse. Controllers: push "
     "the stick of your Hand forward or back to zoom. Hold the grip of your "
     "other hand and pull to move the land, across and up and down. Your "
     "Hand is the right controller, or the left one with a left hand (see "
     "Hand). Mouse: drag the "
     "land to move it. Turn the "
     "wheel to zoom one step. Press a tilt key to move the ground up or "
     "down. Stereoscopic window: the game is on a 3D screen in front of "
     "you, and your head does not move the camera. Tabletop: the island is "
     "a model on a table in front of you. Walk around it and lean over it. "
     "This mode is a test and is not complete."},
    // Not offered: 7.5 units for each meter is the life size scale, and a
    // different value makes the Hand and the world the wrong size.
    // {kInt, "head_units_per_meter_x10", "World scale, tenths per meter", nullptr,
    //  75,
    //  "How many game units a real meter is worth, in tenths. 75 means 7.5 "
    //  "units, which keeps what your eyes see and what your neck feels the same "
    //  "size."},
    {kBool, "intro_vr", "Intro in stereo", nullptr, 0,
     "Shows the Lionhead logo at the start of the game in 3D around you. Your "
     "view moves through the orbs on the path of the game, and it does not "
     "turn: you look where your head looks. The menu button skips it. This "
     "setting is a test. Off shows the intro flat."},
    {kValues, "cutscene_immersive", "Cut scenes",
     "In a window=0|Around you=1", 0,
     "In a window: a cut scene of the game plays on a screen in front of "
     "you, and the land around you stays still. Around you: a cut scene "
     "plays all around you, and the camera of the game moves you with all "
     "its motion. Choose it only if a camera that moves is no problem for "
     "you."},

    // Not offered: the sizes and the depth of the interface in the eye
    // pictures have settled defaults. With controllers the hints and the
    // subtitles are on cards of the mod, which these keys do not change.
    // {kGroup, nullptr, "Comfort", nullptr, 0, nullptr},
    // {kValues, "ui_scale_pct", "Interface size",
    //  "25%=25|30%=30|40%=40|50%=50|75%=75|100%=100", 25,
    //  "The size of subtitles, help text, tooltips and the Creature panel. A "
    //  "smaller size also moves them nearer to the middle of your view. 100% "
    //  "is the size that the game uses. Menus keep their own size."},
    // {kInt, "hint_scale_pct", "Hint size, percent", nullptr, 20,
    //  "The game sizes its hints in pixels, which is small on a monitor and "
    //  "large in a headset. 20 is a fifth of the size the game asks for."},
    // {kBool, "hint_flat", "Draw the hint flat", nullptr, 1,
    //  "The hint stays on the Hand but loses the Hand's depth, so it does not "
    //  "sit a hand's breadth from your face. Turn it off to give it the Hand's "
    //  "own depth again."},
    // {kInt, "ui_shift_px", "Interface depth, pixels", nullptr, 8,
    //  "How far the interface sits in front of you. More pixels is nearer."},
    // {kBool, "ui_adaptive", "Bring the interface forward when needed", nullptr,
    //  1,
    //  "Moves the interface nearer when the world in front of it is nearer, so "
    //  "scenery does not poke through it."},

    {kGroup, nullptr, "Moving around", nullptr, 0, nullptr},
    {kChoice, "snap_turn", "Turn in steps", "Off|30 degrees|45 degrees|90 degrees",
     2,
     "Push the stick of your Hand left or right, drag sideways with the middle "
     "mouse button, or press a turn key, to turn in one step. You stay "
     "where you are. The middle mouse button does not tilt the camera. Off "
     "gives the smooth turn and tilt of the game."},
    {kValues, "god_zoom_smooth", "Zoom with the stick", "In steps=0|Smooth=1",
     0,
     "In steps: one push of the stick of your Hand forward or back is one zoom "
     "step. Smooth: the zoom changes while you hold the stick. It stops when "
     "you let the stick go."},
    {kValues, "god_zoom_forward_in", "Stick forward",
     "Zooms out=0|Zooms in=1", 0,
     "The direction of the zoom on the stick of your Hand. Zooms out: push the "
     "stick forward and the world gets smaller, pull it back and the world "
     "gets larger. Zooms in is the other way."},
    {kValues, "god_zoom_about", "Center of the zoom",
     "The land you look at=1|The land under you=0|Your Hand=2", 1,
     "The point that stays where it is when you zoom with the stick. The "
     "land you look at: the thing in the middle of your view keeps its place "
     "and changes its size. The land under you: you keep your place on the "
     "land. Your Hand: the land at your Hand keeps its place."},
    {kValues, "move_stick", "Move with the other stick",
     "Head direction=1|Controller direction=2|Off=0", 1,
     "Push the stick of your other hand (the left stick, or the right stick "
     "with a left hand) to move over the "
     "land in the God view. You stop when you let the stick go. Head "
     "direction: forward is where your head faces. Controller direction: "
     "forward is where that controller points. Off: the stick does not move "
     "you. Some people feel ill when a stick moves them: use Off then, and "
     "move with the grip and with the teleport."},
    {kValues, "move_stick_follow_land", "Walk follows the land", "On=1|Off=0",
     1,
     "When you move with the stick, the height of your view follows "
     "the hills, so the land stays at the same height below your hands. The "
     "view goes up and down smoothly, and it does not follow each small "
     "bump. It moves only while you push the stick. Off: the view keeps its "
     "height, and a hill comes nearer or goes farther away."},
    // Not offered. These four keys act on the camera of the game. In the
    // God view the mod moves the view in cuts and in its own zoom steps,
    // and a story shows in a window in the room, so the defaults are the
    // only values that a headset needs. The zoom step still acts in the
    // stereoscopic window.
    // {kBool, "instant_jumps", "Jump at once", nullptr, 1,
    //  "A double click on the land takes you there in one frame. Off, the "
    //  "camera flies there as the game does it."},
    // {kBool, "camera_glide", "Let the camera glide", nullptr, 0,
    //  "Off, the camera stops when you release the land. On, it slides on for "
    //  "a moment, as the game does it."},
    // {kChoice, "zoom_step", "Zoom step", "Game|Small|Medium|Large", 2,
    //  "Sets how far one click of the mouse wheel zooms. Medium is about three "
    //  "times the step of the game. On Small, Medium and Large, clicks in the "
    //  "next 0.15 seconds join the next step."},
    // {kChoice, "script_camera", "Story camera",
    //  "Game|Cut at the start|Cut at the end", 2,
    //  "How the camera moves in cutscenes and tutorials. Game: it glides as the "
    //  "game moves it. Cut at the start: the view cuts to where each move goes. "
    //  "Cut at the end: the view holds until the camera stops, then it cuts. "
    //  "With both cut choices, a long move also cuts on the way, a close shot "
    //  "gets a little more room, and the camera does not shake."},
    {kChoice, "blink", "Blink when you jump or turn", "Off|Short|Long", 0,
     "Shows black for a moment when you jump or turn. Short is 0.1 seconds "
     "and long is 0.2 seconds. Some people find a jump easier with a blink."},

    {kGroup, nullptr, "Controllers", nullptr, 0, nullptr},
    {kValues, "controls", "Controls", "Default=0|Trigger is grab=1", 0,
     "Default: the trigger of your Hand clicks, and its grip takes things. "
     "The grip of your other hand moves the world. Trigger is grab: the grips "
     "of both hands only move the world. Hold both grips to turn the world "
     "and to make it larger or smaller, and your height above the land "
     "stays. The trigger of your Hand takes a thing at once, also from a "
     "distance: point, hold the trigger and pull. Where nothing can be "
     "taken, a press of the trigger clicks. Pull the trigger of your other "
     "hand while you move with its stick to move faster, and tilt that "
     "controller up or down to fly up or down. Its X puts the leash on your "
     "Creature and gives the next kind, and a shake of that hand takes the "
     "leash off."},
    // Not offered: the Hand is the real hand. The pointer stays in the mod
    // for a dialog box and for tests (controller_hand=0).
    // {kValues, "controller_hand", "Hand", "Direct hand=1|Pointer=0", 1,
    //  "Direct hand: the Hand is where your controller is. Stretch your arm to "
    //  "reach far into the world. Pointer: the controller points a ray, and the "
    //  "Hand goes where the ray points."},
    {kValues, "left_hand", "Hand", "Follow the game=-1|Right=0|Left=1", -1,
     "Which controller is the Hand. Follow the game: the left handed option "
     "in the options of the game chooses it. Right or Left: the controller "
     "of that hand is the Hand. The other controller grabs the world, moves "
     "you with its stick and opens the ring."},
    {kSlider, "hand_real_cm", "Your hand length", "14|26|1| cm", 19,
     "The length of your real hand, from the wrist to the tip of the middle "
     "finger. Your hands in the game have this length, so they are the size "
     "of your own hands, and so has the ring of the leash. "
     "Measure your hand and set the value here."},
    // THE FIT OF THE HAND ON THE CONTROLLER (docs/hand-model.md). The values
    // are for the right hand, and the left hand gets the mirror image.
    {kSlider, "hand_fit_out_mm", "Hand fit: to the side", "-60|60|2| mm", 0,
     "Moves your hands in the game against your controllers, so that they "
     "cover your real hands. Each controller holds the hand at a different "
     "place. Plus moves each hand away from the other hand, to the back of "
     "the hand. The left hand gets the mirror image of each hand fit value."},
    {kSlider, "hand_fit_up_mm", "Hand fit: up", "-60|60|2| mm", 0,
     "Moves your hands up (plus) or down (minus) against your controllers."},
    {kSlider, "hand_fit_forward_mm", "Hand fit: forward", "-60|60|2| mm", 0,
     "Moves your hands forward (plus) or back (minus) against your "
     "controllers, along the fingers of a closed hand."},
    {kSlider, "hand_fit_pitch_deg", "Hand fit: tilt up", "-45|45|1| degrees",
     0,
     "Turns the front of your hands up (plus) or down (minus) against your "
     "controllers."},
    {kSlider, "hand_fit_yaw_deg", "Hand fit: turn in", "-45|45|1| degrees", 0,
     "Turns the front of your hands toward the other hand (plus) or away "
     "from it (minus)."},
    {kSlider, "hand_fit_roll_deg", "Hand fit: roll in", "-45|45|1| degrees",
     0,
     "Turns the top of your hands toward the other hand (plus) or away from "
     "it (minus), about the line of the fingers."},
    {kValues, "finger_tracking", "Finger tracking",
     "Real fingers=1|Around the controller=2|Off=0", 1,
     "For controllers that see your fingers, for example the controllers of "
     "the Steam Frame and the Valve Index. Real fingers: each finger in the "
     "game bends as your finger bends. Around the controller: the same, but "
     "a closed hand closes around the controller. Off: the trigger bends "
     "the index finger and the grip bends the other fingers. Controllers "
     "that do not see your fingers always use the trigger and the grip."},
    {kBool, "hand_glow", "Glow at the Hand's point", nullptr, 0,
     "A small glow at the tip of the index finger shows the point of the "
     "Hand that picks up and clicks. It shows while a press of the grip "
     "acts on something."},
    {kValues, "grab_hand", "Grab the world",
     "Grip of the other hand=1|Either grip=2|Off=0", 1,
     "Hold the grip of your other hand. Move that hand to pull the world. "
     "Hold both grips to turn the world and to zoom. Either grip: the grip "
     "of your pointing hand also grabs the world when the Hand is empty and "
     "not over an object. Off: the grips do not move the world."},
    {kValues, "hint_learn", "Hints", "Until learned=1|Always=0", 1,
     "The cards beside your wrists show what each button does. Until "
     "learned: a line goes away after you do its action 3 times. It comes "
     "back after 10 minutes of play without that action. In a lesson of the "
     "tutorial, each line shows. Always: each line always shows. To show all "
     "lines again, delete bwvr-hint-counts.ini in the game folder."},

    {kGroup, nullptr, "Launcher", nullptr, 0, nullptr},
    {kBool, "update_check", "Check for updates", nullptr, 1,
     "Each time the launcher opens, it asks GitHub for the latest version "
     "of the mod. When a newer version is out, the line at the bottom of "
     "the window says so. Click that line to open the page of the new "
     "version in your browser. Off: the launcher does not ask."},

    {kGroup, nullptr, "Testing", nullptr, 0, nullptr},
    {kBool, "net_check_skip", "Tell the game that there is no internet",
     nullptr, 1,
     "After the logos the game checks for an internet connection. Its "
     "online features have no server now. In the headset this check can "
     "stop the game on the loading screen. On: the mod tells the game at "
     "once that there is no connection. Off: the game does its own check."},
    {kValues, "motion_comp", "Picture follows the stick (headset app)",
     "Off=0|Newest place=1|Newest place and walk=2", 2,
     "Only the headset app uses this. When the game gives a picture late, "
     "the headset shows a picture again. Off: that picture shows from the "
     "place of the player when the game drew it, so a walk with the stick "
     "stutters. Newest place: the picture shows at its place in the world, "
     "seen from the newest place of the player that the mod gave. Newest "
     "place and walk: the same, and that place goes on with the walk of the "
     "stick to the time when the headset shows the picture. The menu of the "
     "app changes it at once with Save."},
    {kBool, "test_keys", "Test keys", nullptr, 0,
     "Turns on the keys for tests. Each key changes one rule of the mod "
     "while you play, so that you can compare, and the readme lists them. "
     "F12 saves a picture of each eye in the game folder. A player does "
     "not need this."},
    {kInt, "game_sse_parts", "SSE paths of the game (bits)", nullptr, 0,
     "A test of the speed. The game has code with SSE of its own for 13 "
     "parts of its work. 0: the game chooses by the CPU, and the headset "
     "gets the x87 code. Another number is a sum of bits, one bit for each "
     "part: the parts with their bit get the SSE code of the game, the other "
     "parts the x87 code. 1 is the land, 2 and 4 the triangles of the land, "
     "8 and 16 the clip, 32 the sprites, 64 the test of an object on the "
     "screen, 128 and 256 the light of the land, 512 and 1024 the textures "
     "of the land, 2048 a map, 4096 the change of a number to an integer. "
     "The doc backend.md of the mod lists them. A player does not need "
     "this. The change counts from the next start."},
    {kBool, "debug_menu", "Debug menu", nullptr, 0,
     "Adds the item Debug to the ring of your left hand. It opens pages "
     "with the switches of the test keys, and a choice changes a switch at "
     "once. So you can compare in the headset with no keyboard. It does "
     "not need the test keys. A player does not need this."},
};


// Keys that the launcher and the menu of the headset app do not offer. Each
// save deletes them from bwvr.ini (see "Retired keys" in docs/launcher.md).
inline const char* const kRetiredKeys[] = {
    "head_tracking",   "immersive",        "head_fov_deg",
    "eye_separation_milli", "hand_anchored", "head_roll",
    "ui_world_lock",   "pointer_clip",     "pointer_scale",
    "window_width",    "window_height",    "freeze_time",
    "skip_hand_pass2", "skip_physics_pass2", "skip_snapshot",
    "profile",         "capture_every_seconds", "subtitle_lift_pct",
    // Keys for desktop tests. stereo_headless=1 stops the headset session
    // with no message.
    "stereo_headless", "draw_probe",       "draw_probe_frames",
    "draw_probe_delay", "fov_sweep_after", "fov_absolute_deg",
    "fix_unscaled",    "fix_copy",         "fix_inverse",
    "fix_random",      "move_camera_point", "skip_hand_prepare",
    "skip_desktop_present", "hint_mode",   "ui_accounting",
    "filter_survey",   "gdi_watch",        "dump_camera",
    "headless_grab",   "fake_head",        "fake_head_yaw",
    "fake_head_pitch", "fake_head_roll",   "fake_head_sweep",
    "fake_controller", "fake_controller_right_cm", "fake_controller_up_cm",
    "fake_controller_forward_cm", "fake_controller_yaw",
    "fake_controller_pitch", "fake_controller_roll", "fake_controller_sweep",
    "fake_controller_sweep_yaw", "fake_controller_sweep_pitch",
    "fake_controller_script", "fake_controller_points",
    "fake_controller_left_points", "fake_head_right_cm", "fake_head_up_cm",
    "fake_head_forward_cm", "fake_head_fov", "fake_head_layers",
    "grab_probe", "fake_controller_gestures",
    "fake_controller_gesture_cm", "fake_controller_front", "hint_profile",
    "hint_selftest",   "miracle_test_seeds", "miracle_test_cancel",
    "fake_mouse_script", "fake_mouse_step_px", "fake_mouse_x_pct",
    "fake_mouse_y_pct", "fake_mouse_x2_pct", "fake_mouse_y2_pct",
    "radial_test_save", "village_test_chants", "village_test_census",
    "village_test_icon", "village_test_site", "seek_step_cm",
    "village_test_miracles", "village_test_scaffolds",
    "village_test_workshop", "pointer_leave_cursor", "cursor_trace",
    "exit_test_frame", "crash_test_frame", "target_check_test_frame",
    "registry_test_guard", "intro_skip_test_frame",
    "fake_controller_play_frame", "fake_controller_play_after",
    "menu_skip_test_frame", "intro_motion_test_seconds",
    "intro_push_test_px",
    // More keys for desktop tests: each key that the mod reads with _test in
    // its name, the turns and the places of the scripted controller, and the
    // keys of a self test, of a probe and of test pictures.
    "aim_test_keys", "aim_test_touch", "audio_play_fix_test",
    "audio_play_now_test", "audio_open_loader_test",
    "audio_stop_wait_test",
    "audio_test_bank", "audio_test_path", "audio_test_solo",
    "audio_test_step_ms", "audio_test_tag", "audio_test_tag_bank",
    "audio_test_voice", "bulk_test_influence", "bulk_test_no_keep",
    "command_test_class", "creature_command_test",
    "cutscene_depth_test_off", "cutscene_depth_test_on",
    "cutscene_window_test_off", "cutscene_window_test_on",
    "cutscene_window_capture", "far_test_aim_blend_pct",
    "far_test_aim_class", "far_test_aim_toward", "far_test_creature",
    "far_test_forest", "far_test_influence", "far_test_least_cm",
    "far_test_no_trees", "far_test_scroll", "far_test_thrown",
    "far_test_tree", "far_test_aim_blends", "far_test_aim_pair",
    "far_test_quest", "far_test_take_gone", "bubble_card_test",
    "bubble_card_test_frame", "one_walk_marks_test_frame",
    "eye_clean_test", "eye_clean_test_frame",
    "frame_time_test_frame", "frame_time_test_readers",
    "frame_time_test_readers_frames", "god_zoom_about_test",
    "god_zoom_test_toggle",
    "hand_feel_test_toggle", "hand_model_test_look", "hand_model_test_swap",
    "help_test", "hint_counts_test_file", "land_light_test",
    "near_test_flips", "sky_test_frame", "sky_test_trace",
    "sky_test_hour", "spawn_test_flag", "sprite_test_frame",
    "sprite_test_trace",
    "spawn_test_temple",
    "spawn_test_toggle", "spawn_test_x", "spawn_test_z",
    "spirit_point_world_test", "spirit_world_size_test",
    "subtitle_card_test", "subtitle_selftest",
    "subtitle_read_ahead_test_seconds", "profile_sample",
    "profile_sample_thread", "profile_sample_dump", "profile_gpu",
    "hang_watch", "gpu_flush_ab", "ab_test", "d3d7_count", "fast_transform_check",
    "test_cores", "vk_scene", "vk_scene_test_frame", "vk_scene_follow", "vk_eyes", "xr_vulkan_upload", "vk_panel", "vk_only", "vk_only_test", "vk_upload_queue", "vk_scene_reread_test", "ddraw_census", "test_smt", "test_spin_walk_us", "test_spin_idle_us",
    "test_wake_records", "vulkan_fallback_test", "vk_stream_record",
    "vk_stream_frames", "vk_stream_live", "vk_stream_live_drop_us",
    "vk_stream_live_drop", "vk_stream_live_end", "vk_stream_screen_test",
    "render_thread_test_frame",
    "temple_test_camera_trace",
    "temple_test_click_frame", "temple_test_door_on",
    "temple_test_enter_frame", "temple_test_flight_frame",
    "temple_test_follow_frame", "temple_test_scan", "temple_test_scan_frame",
    "temple_test_scan_hall", "temple_test_taps_frame", "temple_test_targets",
    "text_click_test", "text_click_test_text", "fake_controller_rots",
    "fake_controller_turns", "seek_plan", "seek_go", "seek_picture",
    "seek_class", "seek_bulk", "seek_forest", "seek_scroll", "seek_tap",
    "seek_thrown", "intro_probe",
    // Settings that the launcher offered before. Each has one right answer
    // in a headset, and the mod's default is that answer. The other
    // settings that are not offered now (the sizes and the depth of the
    // interface, the hint, the zoom step) stay in the ini as they are, so a
    // value that a person wrote by hand is kept.
    "controller_hand", "controller_left_handed", "script_camera",
    "instant_jumps",
    "camera_glide",    "head_units_per_meter_x10",
};

inline const int kSettingCount =
    static_cast<int>(sizeof(kSettings) / sizeof(kSettings[0]));
inline const int kRetiredKeyCount =
    static_cast<int>(sizeof(kRetiredKeys) / sizeof(kRetiredKeys[0]));

// A slider's range lives in the choices field as "low|high|step", so the
// Setting struct does not need to grow a pair of numbers that only one kind
// would ever use.
inline void ScaleRange(const char* spec, int* low, int* high, int* step) {
  *low = 50;
  *high = 200;
  *step = 5;
  if (spec == nullptr) return;
  int value[3] = {*low, *high, *step};
  int found = 0;
  const char* p = spec;
  while (found < 3 && *p != 0) {
    value[found++] = atoi(p);
    while (*p != 0 && *p != '|') ++p;
    if (*p == '|') ++p;
  }
  if (found >= 1) *low = value[0];
  if (found >= 2) *high = value[1];
  if (found >= 3) *step = value[2];
}

// The unit of a kSlider: the text after the third '|' of its spec. Empty
// when the spec has no unit.
inline const char* SliderUnit(const char* spec) {
  const char* unit = spec;
  for (int bars = 0; unit != nullptr && *unit != 0 && bars < 3; ++unit) {
    if (*unit == '|') ++bars;
    if (bars == 3) {
      ++unit;
      break;
    }
  }
  return unit != nullptr ? unit : "";
}

// A value of a kSlider or a kScale on its step and in its range.
inline int SliderSnap(const char* spec, int value) {
  int low = 0, high = 0, step = 0;
  ScaleRange(spec, &low, &high, &step);
  if (step > 1) value = low + (value - low + step / 2) / step * step;
  if (value < low) value = low;
  if (value > high) value = high;
  return value;
}

// The number of items in a "one|two|three" or a "label=value|..." list.
inline int ChoiceCount(const char* choices) {
  if (choices == nullptr || *choices == 0) return 0;
  int count = 1;
  for (const char* p = choices; *p != 0; ++p) {
    if (*p == '|') ++count;
  }
  return count;
}

// The label of item `index` of a list, into out. A kValues item is
// "label=value", and only the label is shown.
inline void ChoiceLabel(const char* choices, int index, char* out,
                        size_t size) {
  if (size == 0) return;
  out[0] = 0;
  const char* p = choices;
  for (int i = 0; i < index && p != nullptr; ++i) {
    p = strchr(p, '|');
    if (p != nullptr) ++p;
  }
  if (p == nullptr) return;
  size_t n = 0;
  while (p[n] != 0 && p[n] != '|' && p[n] != '=') ++n;
  if (n >= size) n = size - 1;
  memcpy(out, p, n);
  out[n] = 0;
}

// The value of item `index` in a "label=value|label=value" list.
inline int ChoiceValue(const char* choices, int index) {
  const char* p = choices;
  for (int i = 0; i < index && p != nullptr; ++i) {
    p = strchr(p, '|');
    if (p != nullptr) ++p;
  }
  if (p == nullptr) return 0;
  const char* value = strchr(p, '=');
  return value != nullptr ? atoi(value + 1) : 0;
}

// The item whose value is `value`. A value that is not in the list selects
// the largest item below it, so a hand edited ini still shows something near.
inline int ChoiceIndex(const char* choices, int value) {
  int best = 0;
  int best_value = -1;
  int index = 0;
  for (const char* p = choices; p != nullptr; ++index) {
    const char* here = strchr(p, '=');
    const char* next = strchr(p, '|');
    if (here != nullptr && (next == nullptr || here < next)) {
      const int v = atoi(here + 1);
      if (v <= value && v > best_value) {
        best = index;
        best_value = v;
      }
    }
    p = next != nullptr ? next + 1 : nullptr;
  }
  return best;
}

// MulDiv of Windows for numbers that are not negative: number * numerator /
// denominator, rounded to the nearest.
inline int MulDivRound(int number, int numerator, int denominator) {
  if (denominator == 0) return -1;
  const int64_t wide = static_cast<int64_t>(number) * numerator;
  return static_cast<int>((wide + denominator / 2) / denominator);
}

// THE SIZE OF THE RENDER for a resolution scale of `percent`. headset_width
// and headset_height are what the headset asks for, for each eye. margin_pct
// is fov_margin_pct, and one_walk and guard_pct are one_walk and
// one_walk_guard_pct of bwvr.ini.
//
// A square render suits the game best: it derives its vertical angle from the
// render aspect, so a square one covers the headset while wasting the fewest
// pixels. So one number drives both axes, taken from the larger side.
inline void ScaleToPixels(int percent, int headset_width, int headset_height,
                          int margin_pct, bool one_walk, int guard_pct,
                          int* width, int* height) {
  int side = headset_width;
  if (headset_height > side) side = headset_height;
  int pixels = MulDivRound(side, percent, 100);
  // THE MARGIN (stereo.cpp, PrepareHeadView). With fov_margin_pct the game
  // draws that many percent more on each side of the view. The render gets
  // the same percent more pixels each way, so a degree keeps its pixels.
  int margin = margin_pct;
  if (margin < 0) margin = 0;
  if (margin > 30) margin = 30;
  pixels = MulDivRound(pixels, 100 + margin, 100);
  // Keep it even, and inside what the game and the driver will take.
  pixels = (pixels / 2) * 2;
  if (pixels < 640) pixels = 640;
  if (pixels > 4096) pixels = 4096;
  *width = pixels;
  *height = pixels;
  // ONE WALK (docs/backend.md, "One walk for both eyes"). The right eye comes
  // from the drawing of the left eye, and the left eye has nothing past its
  // own right edge. A wider render gives the game a wider horizontal angle at
  // the same pixels per degree (stereo.cpp, PrepareHeadView), so that edge is
  // outside what the right eye shows.
  if (one_walk) {
    int guard = guard_pct;
    if (guard < 0) guard = 0;
    if (guard > 60) guard = 60;
    int wide = MulDivRound(pixels, 100 + guard, 100);
    wide = (wide / 2) * 2;
    if (wide > 4096) wide = 4096;
    *width = wide;
  }
}

// The rows that only the renderer Direct3D 9 uses. With the renderer Vulkan
// they are not shown.
inline bool OnlyDirect3D9(const char* key) {
  return key != nullptr &&
         (strcmp(key, "d3d9_backend") == 0 || strcmp(key, "xr_vulkan") == 0);
}

// The rows that only the launcher on the PC uses. The menu of the headset app
// does not show them, and keeps their values in the ini. A page of that menu
// has room for 10 rows, so the six rows of the hand fit and the three rows of
// the source of the gaze are not on it. In the headset app the fovea follows
// the gaze of OpenXR with the value of foveation_gaze in the ini (1 is the
// default).
inline bool OnlyLauncher(const char* key) {
  return key != nullptr && (strcmp(key, "update_check") == 0 ||
                            strncmp(key, "hand_fit_", 9) == 0 ||
                            strcmp(key, "foveation_gaze") == 0 ||
                            strncmp(key, "eye_osc_", 8) == 0);
}

}  // namespace bwvr_settings
